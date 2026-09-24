// -----------------------------------------------------------------------------
// PRA32-U2 wrapper resampler / sample-rate regression tests (Milestone B).
//
// Compiles the real embedded engine against the desktop shims and drives it
// through the exact resampler the JUCE wrapper uses (PRA32Resampler.h). No JUCE
// is required, so these run in the same lightweight CI job as the engine tests.
//
// Coverage:
//   * 48 kHz passthrough is bit-transparent (fast path, no SRC),
//   * state (phase/history) is continuous across processBlock boundaries,
//   * pitch and musical timing are sample-rate invariant,
//   * 48k -> 44.1k anti-aliasing is far better than the old linear stage,
//   * upsampling interpolation error is far better than the old linear stage,
//   * output is finite/stable and blocks of every size agree,
//   * silence and note-off tails are finite and free of NaN/Inf,
//   * approximate CPU cost per sample rate (reported, not asserted).
// -----------------------------------------------------------------------------

#include "Arduino.h"
#include "EEPROM.h"
#include "I2S.h"

#ifndef PRA32_U2_ENABLE_POLY_ON_1_CORE
#define PRA32_U2_ENABLE_POLY_ON_1_CORE 1
#endif

#ifndef PRA32_U2_USE_EMULATED_EEPROM
#define PRA32_U2_USE_EMULATED_EEPROM 1
#endif

EEPROMClass EEPROM;
I2SClass g_i2s_output;
uint8_t g_midi_ch = 0;

#include "pra32-u2-synth.h"
#include "PRA32Resampler.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using Synth = PRA32_U2_Synth<false, false, false, 0>;
using pra32::Resampler;

namespace {

constexpr double kEngine = pra32::kEngineSampleRate;
constexpr double kPi     = 3.14159265358979323846;

int g_checks = 0;
int g_failures = 0;

void check (bool condition, const char* expression, const char* file, int line)
{
    ++g_checks;

    if (! condition)
    {
        ++g_failures;
        std::printf ("  FAIL  %s:%d  %s\n", file, line, expression);
    }
}

#define CHECK(expr) check ((expr), #expr, __FILE__, __LINE__)

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// A clean, fully sustained patch: instant amp envelope, one saw oscillator,
// no LFO / chorus / delay so measurements are not modulated by effects.
void setAnalyticPatch (Synth& s)
{
    s.control_change (VOICE_MODE, 0);        // POLY
    s.control_change (FILTER_CUTOFF, 127);
    s.control_change (FILTER_RESO, 0);
    s.control_change (FILTER_EG_AMT, 64);
    s.control_change (EG_AMP_MOD, 0);
    s.control_change (AMP_ATTACK, 0);
    s.control_change (AMP_DECAY, 0);
    s.control_change (AMP_SUSTAIN, 127);
    s.control_change (AMP_RELEASE, 0);
    s.control_change (AMP_GAIN, 127);
    s.control_change (OSC_DRIFT, 0);
    s.control_change (LFO_DEPTH, 0);
    s.control_change (CHORUS_MIX, 0);
    s.control_change (DELAY_LEVEL, 0);
    s.control_change (SUSTAIN_PEDAL, 0);
    s.control_change (OSC_1_WAVE, 0);
    s.control_change (OSC_1_MORPH, 0);
    s.control_change (OSC_1_SHAPE, 0);
    s.control_change (MIXER_SUB_OSC, 0);
    s.control_change (MIXER_OSC_MIX, 0);
}

struct EngineSource
{
    Synth synth;

    EngineSource()
    {
        synth.initialize();
        synth.program_change (0);
        setAnalyticPatch (synth);
    }

    Resampler::Sample pull()
    {
        int16_t right = 0;
        int16_t left = synth.process (0, 0, right);
        return { left / 32768.0f, right / 32768.0f };
    }
};

// Render `nHost` host-rate samples of a sustained note through the resampler.
std::vector<float> renderNote (double hostRate, int note, int nHost, int warmup = 0)
{
    EngineSource src;
    src.synth.note_on ((uint8_t) note, 100);

    Resampler r;
    r.prepare (hostRate);

    const int total = warmup + nHost;
    std::vector<float> out;
    out.reserve ((size_t) total);

    for (int i = 0; i < total; ++i)
    {
        const float v = r.process ([&] { return src.pull(); }).l;

        if (i >= warmup)
            out.push_back (v);
    }

    return out;
}

// Hann-windowed single-bin DFT amplitude estimate.
double toneAmp (const std::vector<float>& x, double fs, double f)
{
    const int n = (int) x.size();

    if (n < 4)
        return 0.0;

    double re = 0.0, im = 0.0, wsum = 0.0;
    const double w = 2.0 * kPi * f / fs;

    for (int i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (n - 1));
        re += win * (double) x[(size_t) i] * std::cos (w * i);
        im -= win * (double) x[(size_t) i] * std::sin (w * i);
        wsum += win;
    }

    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

// Box-average decimation, used only to keep the analysis loops cheap; the
// measured fundamentals/rates are far below the decimated Nyquist.
std::vector<float> decimate (const std::vector<float>& x, int factor)
{
    if (factor <= 1)
        return x;

    std::vector<float> out;
    out.reserve (x.size() / (size_t) factor + 1);

    for (size_t i = 0; i + (size_t) factor <= x.size(); i += (size_t) factor)
    {
        float s = 0.0f;
        for (int k = 0; k < factor; ++k) s += x[i + (size_t) k];
        out.push_back (s / (float) factor);
    }

    return out;
}

// Autocorrelation fundamental estimate with parabolic peak interpolation.
double estimateF0 (const std::vector<float>& x, double fs, double fMin, double fMax)
{
    const int n = (int) x.size();

    if (n < 64)
        return 0.0;

    double mean = 0.0;
    for (float v : x) mean += v;
    mean /= (double) n;

    std::vector<double> xc ((size_t) n);
    for (int i = 0; i < n; ++i) xc[(size_t) i] = (double) x[(size_t) i] - mean;

    const int kMin = (int) std::floor (fs / fMax);
    const int kMax = (int) std::ceil (fs / fMin);

    if (kMax >= n || kMin < 1)
        return 0.0;

    std::vector<double> r ((size_t) (kMax + 2), 0.0);
    double best = -1.0e300;
    int bestK = kMin;

    for (int k = kMin; k <= kMax; ++k)
    {
        double s = 0.0;
        for (int i = 0; i + k < n; ++i) s += xc[(size_t) i] * xc[(size_t) (i + k)];

        r[(size_t) k] = s;
        if (s > best) { best = s; bestK = k; }
    }

    double lag = (double) bestK;

    if (bestK > kMin && bestK < kMax)
    {
        const double a = r[(size_t) (bestK - 1)];
        const double b = r[(size_t) bestK];
        const double c = r[(size_t) (bestK + 1)];
        const double denom = a - 2.0 * b + c;

        if (std::abs (denom) > 1.0e-9)
            lag = (double) bestK - 0.5 * (c - a) / denom;
    }

    return fs / lag;
}

double rmsOf (const std::vector<float>& x)
{
    if (x.empty())
        return 0.0;

    double s = 0.0;
    for (float v : x) s += (double) v * (double) v;
    return std::sqrt (s / (double) x.size());
}

bool allFinite (const std::vector<float>& x)
{
    for (float v : x)
        if (! std::isfinite (v))
            return false;

    return true;
}

// Old wrapper resampler, kept only as a test reference for A/B comparisons.
double linearAlias (double hostRate, int nHost, double toneHz)
{
    const double inc = kEngine / hostRate;
    double phase = 1.0;
    long long idx = 0;
    float last = 0.0f, next = 0.0f;

    auto sample = [&] (float& value)
    {
        value = (float) std::sin (2.0 * kPi * toneHz * (double) (idx++) / kEngine);
    };

    sample (next);

    std::vector<float> out;
    out.reserve ((size_t) nHost);

    for (int i = 0; i < nHost; ++i)
    {
        while (phase >= 1.0)
        {
            last = next;
            sample (next);
            phase -= 1.0;
        }

        out.push_back (last + (next - last) * (float) phase);
        phase += inc;
    }

    const double aliasF = std::abs (hostRate - toneHz);
    return toneAmp (out, hostRate, aliasF);
}

// -----------------------------------------------------------------------------
// 1. 48 kHz fast path: bit-transparent
// -----------------------------------------------------------------------------
void testPassthroughIdentity()
{
    std::printf ("48 kHz fast path is bit-transparent...\n");

    Resampler r;
    r.prepare (48000.0);
    CHECK (r.isPassthrough());

    Resampler other;
    other.prepare (44100.0);
    CHECK (! other.isPassthrough());
    other.prepare (96000.0);
    CHECK (! other.isPassthrough());

    // Same event sequence through (a) the bare engine and (b) the resampler:
    // the fast path must forward engine samples unchanged.
    EngineSource a;
    EngineSource b;
    a.synth.note_on (60, 100);
    b.synth.note_on (60, 100);

    Resampler fast;
    fast.prepare (48000.0);

    int mismatches = 0;

    for (int i = 0; i < 20000; ++i)
    {
        const auto direct = a.pull();
        const auto through = fast.process ([&] { return b.pull(); });

        if (direct.l != through.l || direct.r != through.r)
            ++mismatches;
    }

    std::printf ("  bit mismatches=%d\n", mismatches);
    CHECK (mismatches == 0);
    CHECK (fast.getLatencyInHostSamples() == 0.0);
}

// -----------------------------------------------------------------------------
// 2. Continuity across processBlock boundaries
// -----------------------------------------------------------------------------
void testBlockContinuity()
{
    std::printf ("resampler state is continuous across blocks...\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double fs : rates)
    {
        const int total = (int) (fs * 0.2);

        // One-shot reference.
        EngineSource refSource;
        refSource.synth.note_on (64, 100);
        Resampler ref;
        ref.prepare (fs);

        std::vector<float> reference ((size_t) total);
        for (int i = 0; i < total; ++i)
            reference[(size_t) i] = ref.process ([&] { return refSource.pull(); }).l;

        // Split into awkward block sizes; the underlying engine pull sequence is
        // identical, so the output must match sample for sample.
        EngineSource splitSource;
        splitSource.synth.note_on (64, 100);
        Resampler split;
        split.prepare (fs);

        std::vector<float> splitOut ((size_t) total);
        const int blocks[] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
        int written = 0;
        int bi = 0;

        while (written < total)
        {
            const int n = std::min (blocks[bi % 8], total - written);
            ++bi;

            for (int i = 0; i < n; ++i)
                splitOut[(size_t) (written + i)] = split.process ([&] { return splitSource.pull(); }).l;

            written += n;
        }

        double maxDiff = 0.0;
        for (int i = 0; i < total; ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (reference[(size_t) i] - splitOut[(size_t) i]));

        std::printf ("  %7.0f Hz : max block-boundary diff=%.3e\n", fs, maxDiff);
        CHECK (maxDiff == 0.0);
        CHECK (allFinite (reference));
    }
}

// -----------------------------------------------------------------------------
// 3. Pitch is sample-rate invariant
// -----------------------------------------------------------------------------
void testPitchConsistency()
{
    std::printf ("pitch is sample-rate invariant (A4 / C4)...\n");

    struct NoteCase { int note; double expected; const char* name; };
    const NoteCase notes[] = { { 69, 440.0, "A4" }, { 60, 261.6255653, "C4" } };

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (const auto& nc : notes)
    {
        double referenceCents = 0.0;
        double maxSpreadCents = 0.0;

        for (int ri = 0; ri < 4; ++ri)
        {
            const double fs = rates[ri];
            auto y = renderNote (fs, nc.note, (int) (fs * 0.5), (int) (fs * 0.05));

            // Full-rate autocorrelation: decimating first aliases harmonics and
            // biases the parabolic peak, so the (cheap) full-rate estimate is
            // used here. The lag search is bounded to the expected octave.
            const double f0 = estimateF0 (y, fs, nc.expected * 0.6, nc.expected * 1.6);
            const double cents = 1200.0 * std::log2 (f0 / nc.expected);

            if (ri == 0)
                referenceCents = cents;
            else
                maxSpreadCents = std::max (maxSpreadCents, std::abs (cents - referenceCents));

            std::printf ("  %-2s %7.0f Hz : f0=%.5f Hz  %+.3f cents\n", nc.name, fs, f0, cents);
        }

        // The engine's own table quantisation offsets the absolute pitch by
        // ~1 cent; what matters here is that resampling never transposes it.
        // The observed cross-rate spread is < 0.6 cent; allow 2 cents of margin
        // for compiler/FP differences.
        std::printf ("  %-2s cross-rate spread=%.3f cents\n", nc.name, maxSpreadCents);
        CHECK (maxSpreadCents < 2.0);
    }
}

// -----------------------------------------------------------------------------
// 4. Musical timing is sample-rate invariant
// -----------------------------------------------------------------------------
void testTimingConsistency()
{
    std::printf ("timing is sample-rate invariant (onset + LFO period)...\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    double referenceOnset = -1.0;

    for (int ri = 0; ri < 4; ++ri)
    {
        const double fs = rates[ri];
        auto y = renderNote (fs, 69, (int) (fs * 0.25));

        int onset = -1;
        for (int i = 0; i < (int) y.size(); ++i)
            if (std::abs (y[(size_t) i]) > 0.05) { onset = i; break; }

        const double onsetSeconds = onset >= 0 ? (double) onset / fs : -1.0;

        if (ri == 0) referenceOnset = onsetSeconds;

        const double errorMs = std::abs (onsetSeconds - referenceOnset) * 1000.0;
        std::printf ("  %7.0f Hz : onset=%d (%.6f s) error=%.4f ms\n",
                     fs, onset, onsetSeconds, errorMs);

        // Onset is detected within a sample or two of the reference; allow
        // 0.2 ms which is far tighter than any musical perception threshold.
        CHECK (errorMs < 0.2);
    }

    // LFO period: modulate a sustained tone and measure the envelope period.
    // Compare the period in seconds across rates.
    double referencePeriod = -1.0;

    for (int ri = 0; ri < 4; ++ri)
    {
        const double fs = rates[ri];

        EngineSource src;
        setAnalyticPatch (src.synth);
        src.synth.control_change (LFO_DEPTH, 127);
        src.synth.control_change (LFO_OSC_AMT, 127);
        src.synth.control_change (LFO_OSC_DST, 0);   // PITCH
        src.synth.control_change (LFO_RATE, 64);
        src.synth.note_on (69, 100);

        Resampler r;
        r.prepare (fs);

        const int total = (int) (fs * 3.0);
        std::vector<float> env;
        env.reserve ((size_t) total);

        for (int i = 0; i < total; ++i)
        {
            const float v = r.process ([&] { return src.pull(); }).l;
            env.push_back (std::abs (v));
        }

        // Envelope period via autocorrelation of |x| (skip transient). The LFO
        // is slow, so decimating the envelope to ~2 kHz keeps this cheap.
        const int factor = std::max (1, (int) (fs / 2000.0));
        std::vector<float> raw (env.begin() + (size_t) (fs * 0.2), env.end());
        std::vector<float> e = decimate (raw, factor);
        const double fsd = fs / factor;

        double mean = 0.0;
        for (float v : e) mean += v;
        mean /= (double) e.size();
        for (auto& v : e) v -= (float) mean;

        const int kMin = (int) (fsd * 0.05);
        const int kMax = (int) (fsd * 2.0);
        double best = -1.0e300;
        int bestK = kMin;

        for (int k = kMin; k < kMax && k < (int) e.size(); ++k)
        {
            double s = 0.0;
            for (int i = 0; i + k < (int) e.size(); ++i)
                s += (double) e[(size_t) i] * (double) e[(size_t) (i + k)];

            if (s > best) { best = s; bestK = k; }
        }

        const double period = (double) bestK / fsd;

        if (ri == 0) referencePeriod = period;

        std::printf ("  %7.0f Hz : lfo period=%.4f s (error=%.3f ms)\n",
                     fs, period, std::abs (period - referencePeriod) * 1000.0);
        CHECK (std::abs (period - referencePeriod) < 0.005);
    }
}

// -----------------------------------------------------------------------------
// 5. Anti-aliasing (48k -> 44.1k) and interpolation quality
// -----------------------------------------------------------------------------
void testSpectralQuality()
{
    std::printf ("spectral quality of the resampler...\n");

    // (a) 1 kHz passband amplitude must be preserved at every rate.
    {
        const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

        for (double fs : rates)
        {
            Resampler r;
            r.prepare (fs);

            long long idx = 0;
            auto pull = [&]() -> Resampler::Sample
            {
                const float v = (float) std::sin (2.0 * kPi * 1000.0 * (double) (idx++) / kEngine);
                return { v, v };
            };

            for (int i = 0; i < 4800; ++i) r.process (pull);

            std::vector<float> y ((size_t) (fs * 0.4));
            for (auto& v : y) v = r.process (pull).l;

            const double amp = toneAmp (y, fs, 1000.0);
            const double db = 20.0 * std::log10 (amp + 1e-12);
            std::printf ("  passband 1 kHz @ %7.0f Hz : %.4f dB\n", fs, db);
            CHECK (std::abs (db) < 0.1);
        }
    }

    // (b) Downsampling: a 23 kHz engine tone must not alias into the band.
    {
        const double fs = 44100.0;

        Resampler r;
        r.prepare (fs);

        long long idx = 0;
        auto pull = [&]() -> Resampler::Sample
        {
            const float v = (float) std::sin (2.0 * kPi * 23000.0 * (double) (idx++) / kEngine);
            return { v, v };
        };

        for (int i = 0; i < 4800; ++i) r.process (pull);

        std::vector<float> y ((size_t) (fs * 0.4));
        for (auto& v : y) v = r.process (pull).l;

        const double aliasDb = 20.0 * std::log10 (toneAmp (y, fs, fs - 23000.0) + 1e-12);
        const double linearDb = 20.0 * std::log10 (linearAlias (fs, (int) (fs * 0.4), 23000.0) + 1e-12);

        std::printf ("  48k->44.1k 23 kHz alias: new=%.1f dB  linear=%.1f dB\n",
                     aliasDb, linearDb);

        CHECK (aliasDb < -60.0);           // adequate anti-aliasing
        CHECK (aliasDb < linearDb - 30.0); // and clearly better than the old stage
    }

    // (c) Upsampling: interpolation error against the analytic sine must be
    //     tiny and far below the old linear stage.
    {
        const double rates[] = { 96000.0, 192000.0 };

        for (double fs : rates)
        {
            Resampler r;
            r.prepare (fs);

            long long idx = 0;
            auto gen = [&] (double t) { return (float) std::sin (2.0 * kPi * 10000.0 * t); };
            auto pull = [&]() -> Resampler::Sample
            {
                const float v = gen ((double) (idx++) / kEngine);
                return { v, v };
            };

            for (int i = 0; i < 9600; ++i) r.process (pull);

            const int n = (int) (fs * 0.2);
            double errNew = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double ref = std::sin (2.0 * kPi * 10000.0 * (double) i / fs);
                const double v = (double) r.process (pull).l;
                errNew += (v - ref) * (v - ref);
            }
            errNew = std::sqrt (errNew / n);

            // Old linear stage error, measured the same way.
            const double inc = kEngine / fs;
            double phase = 1.0;
            long long lidx = 0;
            float last = 0.0f, next = 0.0f;
            auto lgen = [&] { next = gen ((double) (lidx++) / kEngine); };
            lgen();
            double errLin = 0.0;
            for (int i = 0; i < n; ++i)
            {
                while (phase >= 1.0) { last = next; lgen(); phase -= 1.0; }
                const double v = last + (next - last) * phase;
                const double ref = std::sin (2.0 * kPi * 10000.0 * (double) i / fs);
                errLin += (v - ref) * (v - ref);
                phase += inc;
            }
            errLin = std::sqrt (errLin / n);

            const double dbNew = 20.0 * std::log10 (errNew + 1e-12);
            const double dbLin = 20.0 * std::log10 (errLin + 1e-12);
            std::printf ("  48k->%6.0f Hz interpolation error: new=%.1f dB  linear=%.1f dB\n",
                         fs, dbNew, dbLin);

            CHECK (dbNew < -60.0);
            CHECK (dbNew < dbLin - 30.0);
        }
    }
}

// -----------------------------------------------------------------------------
// 6. Silence / note-off tail behaviour
// -----------------------------------------------------------------------------
void testTailBehaviour()
{
    std::printf ("silence and note-off tail...\n");

    struct Case { const char* name; int delayLevel; int delayFeedback; int delayTime; int release; };
    const Case cases[] = {
        { "delay off",        0,   0,   0,   0  },
        { "delay medium",     64,  64,  64,  64 },
        { "delay high fbk",   100, 127, 120, 0  },
        { "long release",     0,   0,   0,   126 },
    };

    for (const auto& c : cases)
    {
        EngineSource src;
        setAnalyticPatch (src.synth);
        src.synth.control_change (DELAY_LEVEL, c.delayLevel);
        src.synth.control_change (DELAY_FEEDBACK, c.delayFeedback);
        src.synth.control_change (DELAY_TIME, c.delayTime);
        src.synth.control_change (AMP_RELEASE, c.release);
        src.synth.note_on (60, 100);

        Resampler r;
        r.prepare (44100.0);

        // 0.2 s of note, then note off.
        for (int i = 0; i < (int) (44100 * 0.2); ++i) r.process ([&] { return src.pull(); });
        src.synth.note_off (60);

        // 12 s of tail: must be finite and must decay for every bounded case.
        const int n = (int) (44100 * 12.0);
        std::vector<float> tail ((size_t) n);
        for (int i = 0; i < n; ++i) tail[(size_t) i] = r.process ([&] { return src.pull(); }).l;

        CHECK (allFinite (tail));

        const double early = rmsOf ({ tail.begin(), tail.begin() + 4410 });
        const double late  = rmsOf ({ tail.end() - 4410, tail.end() });

        std::printf ("  %-14s early=%.6f late=%.6f\n", c.name, early, late);

        // Everything must fall to a very low level after 12 s (the highest
        // bounded delay feedback decays in ~3.5 s; the highest non-infinite
        // release coefficient in ~17 s, so `release` here is capped at 126 and
        // this case is checked with a looser bound).
        if (c.release <= 0)
            CHECK (late < 1e-3);
        else
            CHECK (late < early);   // still decaying, never growing
    }
}

// -----------------------------------------------------------------------------
// 7. CPU cost (reported, loosely bounded against a runaway regression)
// -----------------------------------------------------------------------------
void testCpuCost()
{
    std::printf ("approximate wrapper CPU cost (4 voices, chorus + delay)...\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double fs : rates)
    {
        EngineSource src;
        src.synth.control_change (CHORUS_MIX, 64);
        src.synth.control_change (CHORUS_RATE, 64);
        src.synth.control_change (CHORUS_DEPTH, 64);
        src.synth.control_change (DELAY_LEVEL, 64);
        src.synth.control_change (DELAY_FEEDBACK, 64);
        src.synth.control_change (DELAY_TIME, 64);
        src.synth.note_on (60, 100);
        src.synth.note_on (64, 100);
        src.synth.note_on (67, 100);
        src.synth.note_on (71, 100);

        Resampler r;
        r.prepare (fs);

        const int n = (int) (fs * 1.0);
        const auto t0 = std::chrono::high_resolution_clock::now();

        volatile float sink = 0.0f;
        for (int i = 0; i < n; ++i)
            sink = sink + r.process ([&] { return src.pull(); }).l;

        const auto t1 = std::chrono::high_resolution_clock::now();
        const double seconds = std::chrono::duration<double> (t1 - t0).count();
        const double realtime = seconds / 1.0;
        const double usPerSample = seconds * 1.0e6 / n;

        std::printf ("  %7.0f Hz : %.3f s / 1.0 s audio  (RT factor %.1fx, %.3f us/sample)  sink=%.3g\n",
                     fs, seconds, 1.0 / realtime, usPerSample, (double) sink);

        // Guard only against a catastrophic regression; the observed cost is
        // ~0.04 s per second of 192 kHz audio even with 4 voices + FX, so 0.5
        // leaves a large margin for slower CI runners while still catching a
        // runaway (e.g. accidentally quadratic) resampler.
        CHECK (realtime < 0.5);
    }
}

} // namespace

int main()
{
    std::printf ("== PRA32-U2 resampler / sample-rate tests ==\n");

    testPassthroughIdentity();
    testBlockContinuity();
    testPitchConsistency();
    testTimingConsistency();
    testSpectralQuality();
    testTailBehaviour();
    testCpuCost();

    std::printf ("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

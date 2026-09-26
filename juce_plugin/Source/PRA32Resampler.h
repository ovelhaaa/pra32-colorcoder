#pragma once

// -----------------------------------------------------------------------------
// PRA32-U2 output resampler (Milestone B / B.1).
//
// The embedded PRA32-U2 engine is authored entirely at a fixed 48 kHz. Its
// oscillator tables, EG/LFO rates, chorus delay line and feedback delay length
// are all expressed in 48 kHz samples, so the only way to preserve the original
// character of the instrument is to keep the DSP core running at 48 kHz and
// resample only the wrapper's output.
//
// This header is deliberately framework-free (no JUCE) so the exact resampler
// used by the plugin can be regression tested standalone.
//
// Behaviour:
//   * host == 48000 Hz  -> passthrough (one engine sample per host sample,
//                          no filtering, no phase accumulator, bit-transparent,
//                          zero added latency)
//   * host  > 48000 Hz  -> causal interpolation
//   * host  < 48000 Hz  -> causal band-limited decimation
//
// Causal design (Milestone B.1)
// -----------------------------
// Earlier revisions used a *symmetric* windowed-sinc kernel centred on the
// current engine sample, which forced the wrapper to synthesise engine samples
// belonging to the future (up to kTaps/2 ahead). That broke sample-accurate
// MIDI: a Note On / CC / Program Change applied at host sample N could already
// have been "heard" by engine samples the kernel had pre-rendered past N.
//
// The kernel is therefore re-phased to be strictly causal: output sample m is
//
//     y[m] = sum_{j=0}^{kTaps-1} e[readIndex - j] * h(j + frac)
//
// where readIndex + frac is the fractional engine position of host sample m and
// h is the same windowed-sinc (Kaiser) prototype, now supported on [0, kTaps).
// Only the current and past engine samples are ever requested, so an event
// applied at host sample N can never influence output produced before N.
//
// The prototype is linear phase with group delay (kTaps - 1) / 2 engine
// samples. That delay is the added latency reported to the host, converted to
// host samples (zero on the 48 kHz fast path).
//
// Filter: windowed-sinc (Kaiser window) evaluated as a polyphase bank with a
// precomputed coefficient table. All coefficients and buffers are allocated in
// prepare(); process() performs no allocation, locks or I/O. Phase/history are
// carried across processBlock() boundaries, so there are no discontinuities at
// block edges.
// -----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pra32
{

inline constexpr double kEngineSampleRate = 48000.0;

class Resampler
{
public:
    struct Sample
    {
        float l = 0.0f;
        float r = 0.0f;
    };

    // Filter design constants. The table is phases * taps floats (shared by both
    // channels), e.g. 512 * 192 * 4 = 384 KiB, allocated once in prepare().
    static constexpr int    kTaps       = 192;
    static constexpr int    kPhases     = 512;
    static constexpr double kKaiserBeta = 8.0;

    // Downsampling keeps a small guard between the passband edge and the new
    // Nyquist so the transition band lands below it. 0.97 places the stopband
    // edge at ~0.99 * hostNyquist for the 48k -> 44.1k case.
    static constexpr double kDownMargin = 0.97;

    // Linear-phase prototype group delay, in engine samples. This is the
    // algorithmic delay the causal kernel adds to the engine stream.
    static constexpr double kGroupDelayEngineSamples = (double) (kTaps - 1) / 2.0;

    Resampler() = default;

    void prepare (double newHostSampleRate)
    {
        hostSampleRate = newHostSampleRate > 0.0 ? newHostSampleRate
                                                 : kEngineSampleRate;

        passthrough = std::abs (hostSampleRate - kEngineSampleRate) < kPassthroughTolerance;
        step        = kEngineSampleRate / hostSampleRate;   // engine samples / host sample

        if (passthrough)
            return;

        buildTable();

        // Ring must hold the whole filter window plus room for the read pointer
        // to advance during one output step (downsampling pulls > 1 sample).
        int needed = kTaps + 64;
        ringSize = 1;
        while (ringSize < needed)
            ringSize <<= 1;
        ringMask = ringSize - 1;

        historyL.assign ((size_t) ringSize, 0.0f);
        historyR.assign ((size_t) ringSize, 0.0f);

        reset();
    }

    void reset() noexcept
    {
        writeIndex = 0;
        outputTime = 0.0;

        std::fill (historyL.begin(), historyL.end(), 0.0f);
        std::fill (historyR.begin(), historyR.end(), 0.0f);
    }

    bool   isPassthrough() const noexcept { return passthrough; }
    double getStep()       const noexcept { return step; }

    // The group delay of the causal prototype, expressed in engine samples.
    double getLatencyInEngineSamples() const noexcept
    {
        return passthrough ? 0.0 : kGroupDelayEngineSamples;
    }

    // Added latency in host samples: the engine-sample group delay scaled by the
    // host/engine rate ratio. Exactly zero on the bit-transparent 48 kHz path.
    double getLatencyInHostSamples() const noexcept
    {
        return passthrough ? 0.0
                           : kGroupDelayEngineSamples * (hostSampleRate / kEngineSampleRate);
    }

    // Produces one host-rate output sample. `pullEngine` is a nullary callable
    // returning an engine-rate Sample (in [-1, 1]); the resampler pulls engine
    // samples strictly as the current/past engine index requires. It never
    // requests an engine sample beyond the one that maps to this output sample.
    template <typename PullEngine>
    inline Sample process (PullEngine&& pullEngine)
    {
        if (passthrough)
            return pullEngine();

        const long long base = (long long) std::floor (outputTime);
        double frac = outputTime - (double) base;

        int phase = (int) (frac * (double) kPhases + 0.5);

        long long readIndex = base;

        if (phase >= kPhases)
        {
            phase = 0;
            ++readIndex;
        }

        // Make sure the history contains the engine sample this output maps to.
        // Never pull beyond it: the kernel is causal, so nothing later is used.
        while ((long long) writeIndex <= readIndex)
        {
            const Sample s = pullEngine();
            historyL[(size_t) writeIndex & (size_t) ringMask] = s.l;
            historyR[(size_t) writeIndex & (size_t) ringMask] = s.r;
            ++writeIndex;
        }

        const float* coeffs = &table[(size_t) phase * (size_t) kTaps];

        float accL = 0.0f;
        float accR = 0.0f;

        long long index = readIndex;

        for (int j = 0; j < kTaps; ++j)
        {
            if (index >= 0)
            {
                const size_t slot = (size_t) index & (size_t) ringMask;
                accL += historyL[slot] * coeffs[j];
                accR += historyR[slot] * coeffs[j];
            }

            --index;
        }

        outputTime += step;

        return { accL, accR };
    }

private:
    static constexpr double kPassthroughTolerance = 1.0e-3;

    static double besselI0 (double x) noexcept
    {
        // Modified Bessel function of the first kind, order 0, by its power
        // series. Only used while building the table (message/prepare thread).
        double sum  = 1.0;
        double term = 1.0;
        const double halfX = x * 0.5;

        for (int k = 1; k < 64; ++k)
        {
            term *= (halfX / (double) k) * (halfX / (double) k);
            sum  += term;

            if (term < 1.0e-16 * sum)
                break;
        }

        return sum;
    }

    static constexpr double kPi = 3.14159265358979323846;

    static double sinc (double x) noexcept
    {
        if (std::abs (x) < 1.0e-12)
            return 1.0;

        const double pix = kPi * x;
        return std::sin (pix) / pix;
    }

    void buildTable()
    {
        const double ratio = hostSampleRate / kEngineSampleRate;

        // Cutoff in cycles/engine-sample. Upsampling reconstructs up to the
        // engine Nyquist; downsampling must band-limit below the host Nyquist.
        double cutoff;

        if (ratio >= 1.0)
            cutoff = 0.5;
        else
            cutoff = 0.5 * ratio * kDownMargin;

        const double i0Beta  = besselI0 (kKaiserBeta);
        const double centre  = kGroupDelayEngineSamples;   // (kTaps - 1) / 2
        const double halfWin = (double) kTaps / 2.0;

        table.assign ((size_t) kPhases * (size_t) kTaps, 0.0f);

        for (int p = 0; p < kPhases; ++p)
        {
            const double frac = (double) p / (double) kPhases;
            float* row = &table[(size_t) p * (size_t) kTaps];

            double sum = 0.0;

            for (int j = 0; j < kTaps; ++j)
            {
                // Causal prototype sampled at x = j + frac, x in [0, kTaps).
                const double x  = (double) j + frac;
                const double tau = x - centre;

                double h = 0.0;

                if (std::abs (tau) < halfWin)
                {
                    const double u   = tau / halfWin;
                    const double win = besselI0 (kKaiserBeta * std::sqrt (std::max (0.0, 1.0 - u * u)))
                                       / i0Beta;
                    h = 2.0 * cutoff * sinc (2.0 * cutoff * tau) * win;
                }

                row[j] = (float) h;
                sum += h;
            }

            // Normalise each phase to unity DC gain so every interpolated
            // position has flat low-frequency response and uniform amplitude.
            if (sum != 0.0)
            {
                const float inv = (float) (1.0 / sum);

                for (int j = 0; j < kTaps; ++j)
                    row[j] *= inv;
            }
        }
    }

    double hostSampleRate = kEngineSampleRate;
    bool   passthrough    = true;
    double step           = 1.0;

    int ringSize = 0;
    int ringMask = 0;

    std::vector<float> table;
    std::vector<float> historyL;
    std::vector<float> historyR;

    std::uint64_t writeIndex = 0;
    double        outputTime = 0.0;
};

} // namespace pra32

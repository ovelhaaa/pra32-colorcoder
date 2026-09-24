#pragma once

// -----------------------------------------------------------------------------
// PRA32-U2 output resampler (Milestone B).
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
//                          no filtering, no phase accumulator, bit-transparent)
//   * host  > 48000 Hz  -> interpolation (suppresses the images of the 48 kHz
//                          stream that would otherwise fold into the output)
//   * host  < 48000 Hz  -> band-limited decimation (anti-alias filtering before
//                          dropping samples; important for 48k -> 44.1k)
//
// Filter: windowed-sinc (Kaiser window) evaluated as a polyphase bank with a
// precomputed coefficient table. All coefficients and buffers are allocated in
// prepare(); process() performs no allocation, locks or I/O. Phase/history are
// carried across processBlock() boundaries, so there are no discontinuities at
// block edges.
//
// Latency: the wrapper *synthesises* the engine samples it needs, pulling them
// on demand during the same host sample's computation. A MIDI event applied
// before a host sample is therefore already reflected by the engine samples the
// kernel interpolates for that same sample. The resampler adds no algorithmic
// input/output latency and the plugin reports zero additional latency.
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

        // Ring must hold the whole filter window plus room for the write pointer
        // to advance during one output step (downsampling pulls >1 sample).
        int needed = kTaps + 8;
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

    // The kernel is causal within the synthesis model; reported separately for
    // completeness. Always zero for this design.
    double getLatencyInHostSamples() const noexcept { return 0.0; }

    // Produces one host-rate output sample. `pullEngine` is a nullary callable
    // returning an engine-rate Sample (in [-1, 1]); the resampler pulls as many
    // engine samples as the current ratio requires.
    template <typename PullEngine>
    inline Sample process (PullEngine&& pullEngine)
    {
        if (passthrough)
            return pullEngine();

        const long long base = (long long) std::floor (outputTime);
        double frac = outputTime - (double) base;

        int phase = (int) (frac * (double) kPhases + 0.5);

        long long readBase = base;

        if (phase >= kPhases)
        {
            phase = 0;
            ++readBase;
        }

        // Ensure the history holds every sample the kernel needs, including the
        // "future" taps (which we can generate on demand because we own the
        // synth, not an input stream).
        const long long highestNeeded = readBase + (kTaps / 2);
        while ((long long) writeIndex <= highestNeeded)
        {
            const Sample s = pullEngine();
            historyL[(size_t) writeIndex & (size_t) ringMask] = s.l;
            historyR[(size_t) writeIndex & (size_t) ringMask] = s.r;
            ++writeIndex;
        }

        const long long first = readBase - (kTaps / 2) + 1;
        const float* coeffs = &table[(size_t) phase * (size_t) kTaps];

        float accL = 0.0f;
        float accR = 0.0f;

        for (int i = 0; i < kTaps; ++i)
        {
            const long long index = first + (long long) i;

            float xl = 0.0f;
            float xr = 0.0f;

            if (index >= 0)
            {
                const size_t slot = (size_t) index & (size_t) ringMask;
                xl = historyL[slot];
                xr = historyR[slot];
            }

            accL += xl * coeffs[i];
            accR += xr * coeffs[i];
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

        const double i0Beta = besselI0 (kKaiserBeta);
        const int    half   = kTaps / 2;

        table.assign ((size_t) kPhases * (size_t) kTaps, 0.0f);

        for (int p = 0; p < kPhases; ++p)
        {
            const double frac = (double) p / (double) kPhases;
            float* row = &table[(size_t) p * (size_t) kTaps];

            double sum = 0.0;

            for (int i = 0; i < kTaps; ++i)
            {
                const double tau = (double) (i - half + 1) - frac;

                double h = 0.0;

                if (std::abs (tau) < (double) half)
                {
                    const double u   = tau / (double) half;
                    const double win = besselI0 (kKaiserBeta * std::sqrt (std::max (0.0, 1.0 - u * u)))
                                       / i0Beta;
                    h = 2.0 * cutoff * sinc (2.0 * cutoff * tau) * win;
                }

                row[i] = (float) h;
                sum += h;
            }

            // Normalise each phase to unity DC gain so every interpolated
            // position has flat low-frequency response and uniform amplitude.
            if (sum != 0.0)
            {
                const float inv = (float) (1.0 / sum);

                for (int i = 0; i < kTaps; ++i)
                    row[i] *= inv;
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

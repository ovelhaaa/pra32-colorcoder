#include "PRA32ValueFormatter.h"

#include <cmath>

namespace {

struct Anchor { double x; double y; };

// Exponential interpolation between documented anchor points. y must be > 0.
double interpExp (const Anchor* a, int count, double x)
{
    if (x <= a[0].x)     return a[0].y;
    if (x >= a[count - 1].x) return a[count - 1].y;

    for (int i = 0; i < count - 1; ++i)
    {
        if (x <= a[i + 1].x)
        {
            const double x0 = a[i].x, x1 = a[i + 1].x;
            const double y0 = a[i].y, y1 = a[i + 1].y;
            const double t = (x - x0) / (x1 - x0);
            return y0 * std::pow (y1 / y0, t);
        }
    }

    return a[count - 1].y;
}

juce::String signedInt (int v)
{
    return v > 0 ? "+" + juce::String (v) : juce::String (v);
}

juce::String asTime (double ms)
{
    if (ms >= 1000.0)
        return juce::String (ms / 1000.0, 2) + " s";

    return juce::String (ms, ms < 10.0 ? 1 : 0) + " ms";
}

juce::String asFrequency (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String (hz, 2) + " Hz";
}

// Filter cutoff: f = 440 * 2^((v-61)/12)  (parameter guide)
double cutoffHzFormula (int v)
{
    return 440.0 * std::pow (2.0, (v - 61) / 12.0);
}

// Filter resonance: Q = 0.7 * 2^(v/32)  (parameter guide)
double resonanceQFormula (int v)
{
    return 0.7 * std::pow (2.0, v / 32.0);
}

// Osc 2 fine detune, engine table collapsed to a piecewise function.
// 256 units == 1 semitone.
double osc2DetuneUnits (int v)
{
    if (v <= 9)   return -3072.0;
    if (v <= 31)  return (v - 32) * 128.0;
    if (v <= 95)  return (v - 64) * 4.0;
    if (v <= 119) return (v - 95) * 128.0;
    return 3072.0;
}

// Osc 2 coarse is simply v - 64 semitones, clamped to +/-60.
int osc2CoarseSemitones (int v)
{
    return juce::jlimit (-60, 60, v - 64);
}

const Anchor attackAnchors[]    = { { 0, 0.7 }, { 32, 6.5 }, { 64, 65.0 }, { 96, 650.0 }, { 127, 6000.0 } };
const Anchor decayAnchors[]     = { { 0, 2.0 }, { 32, 20.0 }, { 64, 200.0 }, { 96, 2000.0 }, { 126, 17300.0 } };
const Anchor releaseAnchors[]   = { { 0, 2.0 }, { 32, 20.0 }, { 64, 200.0 }, { 96, 2000.0 }, { 127, 18600.0 } };
const Anchor fadeAnchors[]      = { { 1, 9.6 }, { 64, 1000.0 }, { 127, 9600.0 } };
const Anchor lfoDepthAnchors[]  = { { 1, 10.8 }, { 64, 50.0 }, { 127, 100.0 } };
const Anchor portaAnchors[]     = { { 1, 1.1 }, { 64, 100.0 }, { 127, 9300.0 } };
const Anchor lfoRateAnchors[]   = { { 1, 0.072 }, { 64, 2.7 }, { 80, 6.9 }, { 127, 103.8 } };
const Anchor chorusRateAnchors[]= { { 0, 0.012 }, { 64, 0.48 }, { 127, 18.4 } };

// Engine delay-time table (samples at 48 kHz), pra32-u2-delay-fx.h.
const int delayTimeTable[128] = {
      48,   96,  144,  192,  240,  288,  384,  480,
     576,  672,  768,  864,  960, 1056, 1152, 1248,
    1344, 1440, 1536, 1632, 1728, 1824, 1920, 2016,
    2112, 2208, 2304, 2400, 2560, 2720, 2880, 3040,
    3200, 3360, 3520, 3680, 3840, 4000, 4160, 4320,
    4480, 4640, 4800, 4960, 5120, 5280, 5440, 5600,
    5760, 5920, 6080, 6240, 6400, 6560, 6720, 6880,
    7040, 7200, 7360, 7520, 7680, 7840, 8000, 8160,
    8320, 8480, 8640, 8800, 8960, 9120, 9280, 9440,
    9600, 9760, 9920, 10080, 10240, 10400, 10560, 10720,
    10880, 11040, 11200, 11360, 11520, 11680, 11840, 12000,
    12160, 12320, 12480, 12640, 12800, 12960, 13120, 13280,
    13440, 13600, 13760, 13920, 14080, 14240, 14400, 14560,
    14720, 14880, 15040, 15200, 15360, 15520, 15680, 15840,
    16160, 16320, 16320, 16320, 16320, 16320, 16320, 16320,
    16320, 16320, 16320, 16320, 16320, 16320, 16320, 16320
};

juce::String formatPitchMod (int v)
{
    using PRA32ParamLogic::PitchModDisplay;

    const auto display = PRA32ParamLogic::classifyPitchMod (v);

    switch (display.unit)
    {
        case PitchModDisplay::Unit::cents:
            return signedInt ((int) std::round (display.amount)) + " ct";

        case PitchModDisplay::Unit::semitones:
            return juce::String (display.amount >= 0 ? "+" : "") + juce::String (display.amount, 2) + " st";

        case PitchModDisplay::Unit::octaves:
            return juce::String (display.amount >= 0 ? "+" : "") + juce::String (display.amount, 2) + " oct";
    }

    return juce::String (v);
}

juce::String enumLabel (const SynthParamData& p, int v)
{
    const int n = p.enumLabels.size();

    if (n == 0)
        return juce::String (v);

    const int index = SynthParameters::enumIndex (p, v);

    return p.enumLabels[juce::jlimit (0, n - 1, index)];
}

} // namespace

juce::String PRA32ValueFormatter::format (const SynthParamData& p, int v)
{
    switch (p.format)
    {
        case PRA32FormatKind::Integer:      return juce::String (v);
        case PRA32FormatKind::Hertz:
            if (p.id == "choRate")
                return asFrequency (v == 0 ? chorusRateAnchors[0].y
                                           : interpExp (chorusRateAnchors, 3, v));
            return v == 0 ? juce::String ("0.00 Hz")
                          : asFrequency (interpExp (lfoRateAnchors, 4, v));
        case PRA32FormatKind::HertzOrKilo:  return asFrequency (cutoffHzFormula (v));
        case PRA32FormatKind::Milliseconds:
        {
            const bool isRelease = (p.id == "egRelease" || p.id == "ampRelease");
            const Anchor* a = isRelease ? releaseAnchors : attackAnchors;
            return asTime (v == 0 ? a[0].y : interpExp (a, 5, v));
        }
        case PRA32FormatKind::MillisecondsOrSeconds:
            return v == 0 ? juce::String ("0 ms")
                          : asTime (interpExp (fadeAnchors, 3, v));
        case PRA32FormatKind::MillisecondsNoDecay:
            return v == 127 ? juce::String ("NO DECAY")
                            : asTime (v == 0 ? decayAnchors[0].y
                                             : interpExp (decayAnchors, 5, v));
        case PRA32FormatKind::Semitones:
            if (p.id == "osc2Coarse") return signedInt (osc2CoarseSemitones (v)) + " st";
            if (p.id == "pbRange")    return juce::String (v) + " st";
            return signedInt (v) + " st";
        case PRA32FormatKind::Detune:
        {
            const double st = osc2DetuneUnits (v) / 256.0;
            if (std::abs (st) < 1.0)
                return signedInt ((int) std::round (st * 100.0)) + " ct";
            return juce::String (st >= 0 ? "+" : "") + juce::String (st, 2) + " st";
        }
        case PRA32FormatKind::ResonanceQ:   return juce::String (resonanceQFormula (v), 2);
        case PRA32FormatKind::Decibels:
            if (v <= 0) return juce::String ("MUTE");
            return juce::String (40.0 * std::log10 ((double) v / 127.0), 1) + " dB";
        case PRA32FormatKind::SignedAmount: return signedInt ((v - 64) * 2);
        case PRA32FormatKind::KeyTrack:
        {
            const double t = v <= 64 ? (v - 64) / 64.0 : (v - 64) / 63.0;
            return juce::String (t >= 0 ? "+" : "") + juce::String (t, 3);
        }
        case PRA32FormatKind::Pan:
        {
            if (v == 64) return juce::String ("CENTER");
            const int amount = v < 64 ? (int) std::round ((64 - v) / 64.0 * 100.0)
                                      : (int) std::round ((v - 64) / 63.0 * 100.0);
            return juce::String (v < 64 ? "L " : "R ") + juce::String (amount) + "%";
        }
        case PRA32FormatKind::PitchModAmount: return formatPitchMod (v);
        case PRA32FormatKind::OscMix:
        {
            if (v == 64) return juce::String ("50 / 50");
            if (v < 64)  return juce::String ("O1 ") + juce::String (juce::jlimit (0, 100, (int) std::round ((64 - v) / 64.0 * 100.0))) + "%";
            return juce::String ("O2 ") + juce::String (juce::jlimit (0, 100, (int) std::round ((v - 64) / 63.0 * 100.0))) + "%";
        }
        case PRA32FormatKind::NoiseSub:
        {
            if (v == 64) return juce::String ("0%");
            if (v < 64)  return juce::String ("N ") + juce::String (juce::jlimit (0, 100, (int) std::round ((64 - v) / 64.0 * 100.0))) + "%";
            return juce::String ("S ") + juce::String (juce::jlimit (0, 100, (int) std::round ((v - 64) / 63.0 * 100.0))) + "%";
        }
        case PRA32FormatKind::Portamento:
            return v == 0 ? juce::String ("0 ms") : asTime (interpExp (portaAnchors, 3, v));
        case PRA32FormatKind::DelayTime:
            return asTime ((double) delayTimeTable[juce::jlimit (0, 127, v)] / 48.0);
        case PRA32FormatKind::ChorusDepth:
            return juce::String::fromUTF8 ("\xC2\xB1") + juce::String (v * (2.7 / 64.0), 1) + " ms";
        case PRA32FormatKind::Enum:         return enumLabel (p, v);
        case PRA32FormatKind::OnOff:        return enumLabel (p, v);
        case PRA32FormatKind::Percent:
            if (p.id == "lfoDepth")
                return v == 0 ? juce::String ("0%")
                              : juce::String (juce::jlimit (0, 100, (int) std::round (interpExp (lfoDepthAnchors, 3, v)))) + "%";
            return juce::String (juce::jlimit (0, 100, (int) std::round (v / 127.0 * 100.0))) + "%";
    }

    return juce::String (v);
}

juce::String PRA32ValueFormatter::formatShort (const SynthParamData& p, int value)
{
    return format (p, value);
}

double PRA32ValueFormatter::cutoffHz (int value)
{
    return cutoffHzFormula (value);
}

double PRA32ValueFormatter::resonanceQ (int value)
{
    return resonanceQFormula (value);
}

float PRA32ValueFormatter::curveProportion (const SynthParamData& p, int v)
{
    switch (p.format)
    {
        case PRA32FormatKind::Milliseconds:
        {
            const bool isRelease = (p.id == "egRelease" || p.id == "ampRelease");
            const Anchor* a = isRelease ? releaseAnchors : attackAnchors;
            const double ms = (v == 0) ? a[0].y : interpExp (a, 5, v);
            const double minMs = a[0].y;
            const double maxMs = a[4].y;
            return (float) juce::jlimit (0.0, 1.0, std::log (ms / minMs) / std::log (maxMs / minMs));
        }

        case PRA32FormatKind::MillisecondsNoDecay:
        {
            if (v >= 127)
                return 1.0f;

            const double ms = (v == 0) ? decayAnchors[0].y : interpExp (decayAnchors, 5, v);
            return (float) juce::jlimit (0.0, 1.0,
                                         std::log (ms / decayAnchors[0].y)
                                             / std::log (17300.0 / decayAnchors[0].y));
        }

        default:
            return juce::jlimit (0.0f, 1.0f, (float) v / 127.0f);
    }
}

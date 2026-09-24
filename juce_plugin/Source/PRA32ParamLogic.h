#pragma once

#include <cmath>
#include <cstdint>

// -----------------------------------------------------------------------------
// Pure, framework-free parameter logic shared by the JUCE presentation layer and
// the automated tests. Nothing here knows about JUCE, the audio buffers or the
// host. The rules mirror the PRA32-U2 engine exactly.
// -----------------------------------------------------------------------------

namespace PRA32ParamLogic
{

// A discrete UI choice described by the inclusive engine CC range it occupies
// plus the canonical CC value the UI should send when that choice is selected.
struct EnumRange
{
    int minValue;
    int maxValue;
    int representativeValue;
};

// --- EG_OSC_DST --------------------------------------------------------------
// update_eg_osc_mod(): 0-12 PITCH, 13-38 CUTOFF, 39-88 PITCH 2, 89-127 SHAPE 1.
inline constexpr EnumRange kEgOscDestRanges[] = {
    {   0,  12,   0 },   // PITCH 1+2
    {  13,  38,  25 },   // CUTOFF
    {  39,  88,  76 },   // PITCH 2
    {  89, 127, 127 }    // SHAPE 1
};
inline constexpr int kEgOscDestRangeCount = 4;

// --- LFO_OSC_DST -------------------------------------------------------------
// update_lfo_osc_mod(): 0-38 PITCH, 39-88 PITCH 2, 89-114 CUTOFF, 115-127 SHAPE 1.
inline constexpr EnumRange kLfoOscDestRanges[] = {
    {   0,  38,   0 },   // PITCH 1+2
    {  39,  88,  76 },   // PITCH 2
    {  89, 114, 100 },   // CUTOFF
    { 115, 127, 127 }    // SHAPE 1
};
inline constexpr int kLfoOscDestRangeCount = 4;

// --- VOICE_MODE --------------------------------------------------------------
// set_voice_mode() maps six CC bands onto four musical modes:
//   0-38 POLY, 39-88 MONO, 89-114 LEGATO+PORTA, 115-127 LEGATO.
inline constexpr EnumRange kVoiceModeRanges[] = {
    {   0,  38,   0 },   // POLY
    {  39,  88,  76 },   // MONO
    {  89, 114, 102 },   // LEGATO + PORTA
    { 115, 127, 127 }    // LEGATO
};
inline constexpr int kVoiceModeRangeCount = 4;

// Index of the range that contains `value`. Falls back to the nearest edge.
inline int enumIndexFromRanges (const EnumRange* ranges, int count, int value)
{
    if (count <= 0)
        return 0;

    for (int i = 0; i < count; ++i)
        if (value >= ranges[i].minValue && value <= ranges[i].maxValue)
            return i;

    return value < ranges[0].minValue ? 0 : count - 1;
}

// -----------------------------------------------------------------------------
// Pitch modulation amount (EG_OSC_AMT / LFO_OSC_AMT). The engine's amount table
// is expressed in cents; the display classifies it into cents / semitones / octaves.
// -----------------------------------------------------------------------------
struct PitchModDisplay
{
    enum class Unit { cents, semitones, octaves };

    double amount;
    Unit unit;
};

struct Anchor { double x; double y; };

inline double lerp (const Anchor* a, int count, double x)
{
    if (x <= a[0].x)         return a[0].y;
    if (x >= a[count - 1].x) return a[count - 1].y;

    for (int i = 0; i < count - 1; ++i)
    {
        if (x <= a[i + 1].x)
        {
            const double t = (x - a[i].x) / (a[i + 1].x - a[i].x);
            return a[i].y + t * (a[i + 1].y - a[i].y);
        }
    }

    return a[count - 1].y;
}

// Engine pitch-mod amount table (cents) for a 0..127 CC value.
inline double pitchModCents (int v)
{
    static const Anchor centsAnchors[] = {
        { 1, -12000 }, { 2, -9600 }, { 3, -7200 }, { 4, -6000 }, { 5, -4800 },
        { 6, -4200 }, { 7, -3600 }, { 8, -3000 }, { 9, -2400 }, { 21, -1200 },
        { 31, -200 }, { 32, -100 }, { 64, 0 }, { 96, 100 }, { 97, 200 },
        { 107, 1200 }, { 119, 2400 }, { 120, 3000 }, { 121, 3600 }, { 122, 4200 },
        { 123, 4800 }, { 124, 6000 }, { 125, 7200 }, { 126, 9600 }, { 127, 12000 }
    };

    return lerp (centsAnchors, (int) (sizeof (centsAnchors) / sizeof (centsAnchors[0])), v);
}

inline PitchModDisplay classifyPitchMod (int v)
{
    const double cents = pitchModCents (v);
    const double st    = cents / 100.0;

    if (std::abs (cents) < 100.0)
        return { cents, PitchModDisplay::Unit::cents };

    if (std::abs (st) < 12.0)
        return { st, PitchModDisplay::Unit::semitones };

    return { st / 12.0, PitchModDisplay::Unit::octaves };
}

} // namespace PRA32ParamLogic

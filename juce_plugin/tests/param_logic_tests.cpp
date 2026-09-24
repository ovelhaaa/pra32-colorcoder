// -----------------------------------------------------------------------------
// PRA32-U2 parameter logic regression tests (Milestone A).
//
// Pure, JUCE-free tests for the pitch-module formatter classification and the
// non-uniform enum ranges shared by the UI and the formatter.
// -----------------------------------------------------------------------------

#include "PRA32ParamLogic.h"

#include <cmath>
#include <cstdio>

namespace {

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
#define CHECK_EQ(a, b) do { auto va = (a); auto vb = (b); \
    if (va != vb) std::printf ("  (expected %ld got %ld)\n", (long) vb, (long) va); \
    CHECK (va == vb); } while (0)

using namespace PRA32ParamLogic;

void testPitchMod()
{
    std::printf ("pitch-mod formatter classification...\n");

    struct Case { int value; PitchModDisplay::Unit unit; double amount; };
    const Case cases[] = {
        {  21, PitchModDisplay::Unit::octaves,   -1.0  },  // -1200 ct
        {  32, PitchModDisplay::Unit::semitones, -1.0  },  // -100 ct
        {  48, PitchModDisplay::Unit::cents,    -50.0  },  // -50 ct
        {  64, PitchModDisplay::Unit::cents,      0.0  },  // 0
        {  80, PitchModDisplay::Unit::cents,     50.0  },  // +50 ct
        {  96, PitchModDisplay::Unit::semitones,  1.0  },  // +100 ct
        { 107, PitchModDisplay::Unit::octaves,    1.0  },  // +1200 ct
        { 119, PitchModDisplay::Unit::octaves,    2.0  },  // +2400 ct
    };

    for (const auto& c : cases)
    {
        const auto d = classifyPitchMod (c.value);
        std::printf ("  v=%3d -> unit=%d amount=%.3f\n", c.value, (int) d.unit, d.amount);
        CHECK (d.unit == c.unit);
        CHECK (std::abs (d.amount - c.amount) < 1e-6);
    }

    // The classification threshold is in cents, not semitones: values strictly
    // inside +/-100 cents stay in cents, values outside become semitones.
    CHECK (classifyPitchMod (64).unit == PitchModDisplay::Unit::cents);
    CHECK (classifyPitchMod (97).unit == PitchModDisplay::Unit::semitones);
    CHECK (classifyPitchMod (96).unit == PitchModDisplay::Unit::semitones);
}

void testRange (const EnumRange* ranges, int count, const int* probes, const int* expectedIndexes, int probeCount,
                const char* name)
{
    std::printf ("%s enum ranges...\n", name);

    for (int i = 0; i < probeCount; ++i)
    {
        const int index = enumIndexFromRanges (ranges, count, probes[i]);
        if (index != expectedIndexes[i])
            std::printf ("  value=%d expected index %d got %d\n", probes[i], expectedIndexes[i], index);
        CHECK_EQ (index, expectedIndexes[i]);
    }

    // Every representative value must round-trip to its own choice.
    for (int i = 0; i < count; ++i)
        CHECK_EQ (enumIndexFromRanges (ranges, count, ranges[i].representativeValue), i);
}

void testEnumRanges()
{
    const int egProbes[]       = { 0, 12, 13, 38, 39, 88, 89, 127 };
    const int egExpected[]     = { 0,  0,  1,  1,  2,  2,  3,   3 };
    testRange (kEgOscDestRanges, kEgOscDestRangeCount, egProbes, egExpected, 8, "EG_OSC_DST");

    const int lfoProbes[]      = { 0, 38, 39, 88, 89, 114, 115, 127 };
    const int lfoExpected[]    = { 0,  0,  1,  1,  2,   2,   3,   3 };
    testRange (kLfoOscDestRanges, kLfoOscDestRangeCount, lfoProbes, lfoExpected, 8, "LFO_OSC_DST");

    const int voiceProbes[]    = { 0, 38, 39, 88, 89, 114, 115, 127 };
    const int voiceExpected[]  = { 0,  0,  1,  1,  2,   2,   3,   3 };
    testRange (kVoiceModeRanges, kVoiceModeRangeCount, voiceProbes, voiceExpected, 8, "VOICE_MODE");
}

// Verifies the representatives actually select the intended engine band when the
// engine's own 6-way index formula is applied.
void testVoiceModeEngineBands()
{
    std::printf ("voice-mode representative -> engine band...\n");

    // engine: idx = (value * 10 + 127) / 254
    // table: { POLY, POLY, MONO, MONO, LEGATO_PORTA, LEGATO }
    const int expectedBand[4] = { 0 /*POLY*/, 3 /*MONO*/, 4 /*LEGATO_PORTA*/, 5 /*LEGATO*/ };

    for (int i = 0; i < kVoiceModeRangeCount; ++i)
    {
        const int value = kVoiceModeRanges[i].representativeValue;
        const int band = (value * 10 + 127) / 254;
        std::printf ("  rep=%3d -> band=%d\n", value, band);
        CHECK_EQ (band, expectedBand[i]);
    }
}

} // namespace

int main()
{
    std::printf ("== PRA32-U2 parameter logic tests ==\n");

    testPitchMod();
    testEnumRanges();
    testVoiceModeEngineBands();

    std::printf ("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

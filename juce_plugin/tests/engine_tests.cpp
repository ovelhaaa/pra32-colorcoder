// -----------------------------------------------------------------------------
// PRA32-U2 engine regression tests (Milestone A).
//
// Compiles the real embedded engine against the lightweight desktop shims in
// `web_app/`. Verifies the P0 polyphony fix and the engine-level MIDI/voice
// behaviour the JUCE wrapper relies on. JUCE is deliberately not required here.
// -----------------------------------------------------------------------------

#include "Arduino.h"
#include "EEPROM.h"
#include "I2S.h"

// Mirror the JUCE target configuration.
#ifndef PRA32_U2_ENABLE_POLY_ON_1_CORE
#define PRA32_U2_ENABLE_POLY_ON_1_CORE 1
#endif

#ifndef PRA32_U2_USE_EMULATED_EEPROM
#define PRA32_U2_USE_EMULATED_EEPROM 1
#endif

// Definitions normally supplied by the Arduino sketch / wrapper.
EEPROMClass EEPROM;
I2SClass g_i2s_output;
uint8_t g_midi_ch = 0;

#include "pra32-u2-synth.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using Synth = PRA32_U2_Synth<false, false, false, 0>;

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

std::unique_ptr<Synth> makeSynth()
{
    auto synth = std::make_unique<Synth>();
    synth->initialize();
    synth->program_change (0);
    return synth;
}

// A deterministic, envelope-stable patch used by the energy comparisons.
void setFlatPatch (Synth& s)
{
    s.control_change (VOICE_MODE, 0);        // POLY
    s.control_change (FILTER_CUTOFF, 127);
    s.control_change (FILTER_RESO, 0);
    s.control_change (FILTER_EG_AMT, 64);
    s.control_change (EG_AMP_MOD, 0);        // separate amp envelope
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
}

std::vector<int16_t> renderLeft (Synth& s, int numSamples)
{
    std::vector<int16_t> out ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        int16_t right = 0;
        out[(size_t) i] = s.process (0, 0, right);
    }

    return out;
}

double rms (const std::vector<int16_t>& samples)
{
    if (samples.empty())
        return 0.0;

    double sum = 0.0;

    for (auto v : samples)
        sum += (double) v * (double) v;

    return std::sqrt (sum / (double) samples.size());
}

void testPolyphony()
{
    std::printf ("polyphony...\n");

    auto single = makeSynth();
    setFlatPatch (*single);
    single->note_on (60, 100);
    const double e1 = rms (renderLeft (*single, 6000));
    CHECK (e1 > 100.0);

    single->all_notes_off();
    renderLeft (*single, 4000);

    auto poly = makeSynth();
    setFlatPatch (*poly);
    poly->note_on (60, 100);
    poly->note_on (64, 100);
    poly->note_on (67, 100);
    poly->note_on (71, 100);
    const double e4 = rms (renderLeft (*poly, 6000));

    std::printf ("  single=%.1f  four=%.1f  ratio=%.2f\n", e1, e4, e4 / e1);
    CHECK (e4 > e1 * 1.5);   // genuinely multiple voices, not one

    // Voice stealing: a fifth note must not silence the instrument.
    poly->note_on (72, 100);
    const double e5 = rms (renderLeft (*poly, 6000));
    std::printf ("  five=%.1f  ratio=%.2f\n", e5, e5 / e1);
    CHECK (e5 > e1 * 1.5);

    // Releasing one note must not stop the others.
    poly->note_off (60);
    const double eAfterRelease = rms (renderLeft (*poly, 6000));
    std::printf ("  after-release=%.1f\n", eAfterRelease);
    CHECK (eAfterRelease > e1 * 1.5);
}

void testSustain()
{
    std::printf ("sustain pedal...\n");

    auto s = makeSynth();
    setFlatPatch (*s);
    s->note_on (60, 100);
    const double eOn = rms (renderLeft (*s, 6000));

    s->control_change (SUSTAIN_PEDAL, 127);  // pedal down
    s->note_off (60);                        // released while held
    const double eHeld = rms (renderLeft (*s, 6000));
    std::printf ("  on=%.1f  held=%.1f\n", eOn, eHeld);
    CHECK (eHeld > eOn * 0.5);               // still sounding

    s->control_change (SUSTAIN_PEDAL, 0);    // pedal up -> release
    const double eReleased = rms (renderLeft (*s, 12000));
    std::printf ("  released=%.1f\n", eReleased);
    CHECK (eReleased < eHeld * 0.5);
}

void testVoiceModes()
{
    std::printf ("voice modes...\n");

    const uint8_t modes[4] = { VOICE_POLYPHONIC, VOICE_MONOPHONIC,
                               VOICE_LEGATO_PORTA, VOICE_LEGATO };
    const char* names[4] = { "POLY", "MONO", "LEGATO+PORTA", "LEGATO" };

    for (int m = 0; m < 4; ++m)
    {
        auto s = makeSynth();
        setFlatPatch (*s);
        s->control_change (VOICE_MODE, modes[m]);
        s->control_change (PORTAMENTO, 32);

        s->note_on (60, 100);
        const double e1 = rms (renderLeft (*s, 4000));
        s->note_on (64, 100);
        const double e2 = rms (renderLeft (*s, 4000));

        std::printf ("  %-12s one=%.1f two=%.1f\n", names[m], e1, e2);
        CHECK (e1 > 100.0);
        CHECK (e2 > 100.0);
    }
}

void testProgramChange()
{
    std::printf ("program change / preset table...\n");

    auto s = makeSynth();

    s->program_change (0);
    CHECK (s->current_controller_value (FILTER_CUTOFF) == 112);
    CHECK (s->current_controller_value (FILTER_RESO) == 48);
    CHECK (s->current_controller_value (AMP_GAIN) == 100);
    CHECK (s->current_controller_value (VOICE_MODE) == 0);
    CHECK (s->current_controller_value (PORTAMENTO) == 48);
    CHECK (s->current_controller_value (OSC_2_PITCH) == 72);

    s->program_change (15);
    CHECK (s->current_controller_value (VOICE_MODE) == 127);
    CHECK (s->current_controller_value (FILTER_CUTOFF) == 127);

    // The wrapper must start on program 0 (INITIALIZATION), never program 15.
    auto startup = makeSynth();
    CHECK (startup->current_controller_value (VOICE_MODE) == 0);
    CHECK (startup->current_controller_value (FILTER_CUTOFF) == 112);
}

void testPitchBend()
{
    std::printf ("pitch bend...\n");

    auto s = makeSynth();
    setFlatPatch (*s);
    s->note_on (60, 100);
    renderLeft (*s, 2000);

    s->pitch_bend ((uint8_t) (8192 & 0x7F), (uint8_t) ((8192 >> 7) & 0x7F)); // centre
    const double centre = rms (renderLeft (*s, 2000));

    s->pitch_bend ((uint8_t) (16383 & 0x7F), (uint8_t) ((16383 >> 7) & 0x7F)); // max
    const double up = rms (renderLeft (*s, 2000));

    s->pitch_bend ((uint8_t) (0 & 0x7F), (uint8_t) ((0 >> 7) & 0x7F)); // min
    const double down = rms (renderLeft (*s, 2000));

    std::printf ("  centre=%.1f up=%.1f down=%.1f\n", centre, up, down);
    CHECK (centre > 100.0);
    CHECK (up > 100.0);
    CHECK (down > 100.0);
}

void testAftertouch()
{
    std::printf ("channel aftertouch...\n");

    auto configure = [] (Synth& s)
    {
        setFlatPatch (s);
        s.control_change (AFT_T_LFO_AMT, 127);
        s.control_change (LFO_DEPTH, 0);     // pressure is the only depth source
        s.control_change (LFO_OSC_AMT, 127);
        s.control_change (LFO_OSC_DST, 0);   // PITCH
        s.control_change (LFO_RATE, 110);    // fast enough to move inside the window
        s.note_on (60, 100);
    };

    auto noPressure = makeSynth();
    configure (*noPressure);
    noPressure->after_touch_channel (0);
    const auto a = renderLeft (*noPressure, 8000);

    auto fullPressure = makeSynth();
    configure (*fullPressure);
    fullPressure->after_touch_channel (127);
    const auto b = renderLeft (*fullPressure, 8000);

    double diff = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        diff += std::abs ((double) a[i] - (double) b[i]);

    const double meanDiff = diff / (double) a.size();
    std::printf ("  mean-diff=%.2f\n", meanDiff);
    CHECK (meanDiff > 10.0);   // pressure actually changes the signal
}

void testMonoCoreIsPoly()
{
    std::printf ("polyphony macro is active...\n");

#ifdef PRA32_U2_ENABLE_POLY_ON_1_CORE
    // If the macro were missing, set_voice_mode would collapse every request to
    // monophonic and this ratio would be ~1.0.
    auto s = makeSynth();
    setFlatPatch (*s);
    s->note_on (60, 100);
    const double e1 = rms (renderLeft (*s, 6000));

    auto p = makeSynth();
    setFlatPatch (*p);
    p->note_on (60, 100);
    p->note_on (64, 100);
    p->note_on (67, 100);
    const double e3 = rms (renderLeft (*p, 6000));
    std::printf ("  one=%.1f three=%.1f\n", e1, e3);
    CHECK (e3 > e1 * 1.4);
#else
    CHECK (false);
#endif
}

} // namespace

int main()
{
    std::printf ("== PRA32-U2 engine tests ==\n");

    testPolyphony();
    testSustain();
    testVoiceModes();
    testProgramChange();
    testPitchBend();
    testAftertouch();
    testMonoCoreIsPoly();

    std::printf ("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

// -----------------------------------------------------------------------------
// JUCE-level plugin regression tests (Milestone B).
//
// These exercise the real PRA32ColorcoderAudioProcessor: host state roundtrip,
// JSON preset roundtrip, block-boundary automation, and the guarantee that the
// very first render after a state restore already uses the restored patch.
//
// This target deliberately compiles the plugin sources against JUCE (unlike the
// framework-free engine/midi/resampler tests) because APVTS serialisation is a
// JUCE concern that cannot be faithfully modelled without it.
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <vector>

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

std::vector<int> snapshot (PRA32ColorcoderAudioProcessor& p)
{
    std::vector<int> values;

    for (const auto& pd : SynthParameters::getParameters())
        values.push_back ((int) std::lround (p.getAPVTS().getRawParameterValue (pd.id)->load()));

    return values;
}

void setParam (PRA32ColorcoderAudioProcessor& p, const juce::String& id, int value)
{
    if (auto* param = p.getAPVTS().getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) value));
}

// Deterministic spanning pattern over *every* parameter, so enums, toggles,
// bipolar values, FX parameters and voice mode are all covered by a roundtrip.
void setPattern (PRA32ColorcoderAudioProcessor& p, int seed)
{
    const auto& ps = SynthParameters::getParameters();

    for (size_t i = 0; i < ps.size(); ++i)
    {
        const int span  = ps[i].max - ps[i].min + 1;
        const int value = ps[i].min + (int) ((i * 7 + (size_t) seed * 13) % (size_t) span);
        setParam (p, ps[i].id, value);
    }
}

void setSustainPatch (PRA32ColorcoderAudioProcessor& p, int cutoff)
{
    setParam (p, "ampAttack", 0);
    setParam (p, "ampDecay", 0);
    setParam (p, "ampSustain", 127);
    setParam (p, "ampRelease", 0);
    setParam (p, "ampGain", 127);
    setParam (p, "ampVelSens", 0);
    setParam (p, "filterReso", 0);
    setParam (p, "egFltAmt", 64);
    setParam (p, "filterCutoff", cutoff);
}

double renderRms (PRA32ColorcoderAudioProcessor& p, int n)
{
    juce::AudioBuffer<float> buffer (2, n);
    buffer.clear();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

    p.processBlock (buffer, midi);

    return (double) buffer.getRMSLevel (0, 0, n);
}

void testTailLength()
{
    std::printf ("getTailLengthSeconds reports a real tail...\n");

    PRA32ColorcoderAudioProcessor p;
    const double tail = p.getTailLengthSeconds();

    std::printf ("  tail=%.1f s\n", tail);
    CHECK (tail > 0.0);
    CHECK (std::isfinite (tail));
}

void testStateRoundtrip()
{
    std::printf ("host state roundtrip preserves every parameter...\n");

    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    setPattern (p, 3);
    const auto a = snapshot (p);

    juce::MemoryBlock state;
    p.getStateInformation (state);
    CHECK (state.getSize() > 0);

    setPattern (p, 9);
    const auto b = snapshot (p);
    CHECK (a != b);

    p.setStateInformation (state.getData(), (int) state.getSize());
    const auto c = snapshot (p);

    CHECK (c == a);
    CHECK (! p.isCurrentPatchEdited());
}

void testJsonRoundtrip()
{
    std::printf ("JSON preset roundtrip (save -> modify -> load)...\n");

    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (44100.0, 256);

    setPattern (p, 5);
    const auto a = snapshot (p);

    const juce::String json = p.savePresetToJson();
    CHECK (json.isNotEmpty());

    setPattern (p, 11);
    const auto b = snapshot (p);
    CHECK (a != b);

    p.loadPresetFromJson (json);
    const auto c = snapshot (p);

    CHECK (c == a);
    CHECK (! p.isCurrentPatchEdited());

    // Loading the same JSON twice must be idempotent.
    p.loadPresetFromJson (json);
    CHECK (snapshot (p) == a);
}

void testBlockBoundaryAutomation()
{
    std::printf ("block-boundary host automation reaches the engine...\n");

    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    setSustainPatch (p, 127);
    const double rmsOpen = renderRms (p, 8192);

    setSustainPatch (p, 12);
    const double rmsClosed = renderRms (p, 8192);

    std::printf ("  rms open=%.5f  closed=%.5f\n", rmsOpen, rmsClosed);

    CHECK (std::isfinite (rmsOpen));
    CHECK (std::isfinite (rmsClosed));
    CHECK (rmsOpen > rmsClosed * 1.5);

    // The APVTS must reflect exactly the value the host wrote.
    CHECK ((int) std::lround (p.getAPVTS().getRawParameterValue ("filterCutoff")->load()) == 12);
}

void testFirstRenderAfterRestore()
{
    std::printf ("first render after state restore uses the restored patch...\n");

    PRA32ColorcoderAudioProcessor reference;
    reference.prepareToPlay (48000.0, 512);
    setSustainPatch (reference, 12);
    const double rmsRef = renderRms (reference, 8192);

    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    setSustainPatch (p, 12);

    juce::MemoryBlock state;
    p.getStateInformation (state);

    setParam (p, "filterCutoff", 127);            // mutate away from the captured patch
    p.setStateInformation (state.getData(), (int) state.getSize());

    const double rmsRestored = renderRms (p, 8192);

    std::printf ("  reference=%.6f  restored=%.6f\n", rmsRef, rmsRestored);

    // The first block after the restore must already be the restored patch, not
    // a block rendered with the stale value that was overwritten.
    CHECK (std::abs (rmsRestored - rmsRef) < 1.0e-5);
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("== PRA32-U2 plugin (JUCE) tests ==\n");

    testTailLength();
    testStateRoundtrip();
    testJsonRoundtrip();
    testBlockBoundaryAutomation();
    testFirstRenderAfterRestore();

    std::printf ("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

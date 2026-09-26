// -----------------------------------------------------------------------------
// JUCE-level plugin regression tests (Milestone B / B.1).
//
// These exercise the real PRA32ColorcoderAudioProcessor: host state roundtrip,
// JSON preset roundtrip, block-boundary automation, the canonical factory
// program table, sample-accurate MIDI through the causal resampler, and the
// guarantee that the very first render after a state restore already uses the
// restored patch at every supported sample rate.
//
// This target deliberately compiles the plugin sources against JUCE (unlike the
// framework-free engine/midi/resampler tests) because APVTS serialisation is a
// JUCE concern that cannot be faithfully modelled without it.
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FactoryProgramTable.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#ifndef PRA32_REPO_ROOT
#define PRA32_REPO_ROOT "."
#endif

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

// Renders one block, injecting `midi`, and returns the left channel as a vector.
std::vector<float> renderBlock (PRA32ColorcoderAudioProcessor& p, int n,
                                juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> buffer (2, n);
    buffer.clear();
    p.processBlock (buffer, midi);

    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i)
        out[(size_t) i] = buffer.getSample (0, i);

    return out;
}

// -----------------------------------------------------------------------------
// Existing behaviour
// -----------------------------------------------------------------------------
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

void testFirstRenderAfterRestore (double sampleRate)
{
    std::printf ("first render after state restore uses the restored patch (%.0f Hz)...\n",
                 sampleRate);

    PRA32ColorcoderAudioProcessor reference;
    reference.prepareToPlay (sampleRate, 512);
    setSustainPatch (reference, 12);
    const double rmsRef = renderRms (reference, 8192);

    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (sampleRate, 512);
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

// -----------------------------------------------------------------------------
// Canonical factory program table
// -----------------------------------------------------------------------------
void testFactoryTableStructure()
{
    std::printf ("factory table matches SynthParameters layout and defaults...\n");

    const auto& ps = SynthParameters::getParameters();

    CHECK ((int) ps.size() == FactoryPrograms::kParameterCount);
    CHECK (FactoryPrograms::kProgramCount == PRA32MidiState::kFactoryProgramCount);
    CHECK (FactoryPrograms::kProgramCount == 26);

    int mismatches = 0;

    for (int i = 0; i < FactoryPrograms::kParameterCount; ++i)
    {
        const auto& row = FactoryPrograms::kRows[(size_t) i];
        const auto& pd  = ps[(size_t) i];

        if (pd.id != juce::String (row.parameterId))            ++mismatches;
        if (pd.presetKey != juce::String (row.presetKey))       ++mismatches;
        if (pd.cc != row.cc)                                    ++mismatches;
        if (pd.def != (int) row.values[0])                      ++mismatches;
    }

    std::printf ("  layout mismatches=%d\n", mismatches);
    CHECK (mismatches == 0);
}

int compareJsonToTable (const juce::var& parsed)
{
    auto* obj = parsed.getDynamicObject();

    if (obj == nullptr)
        return -1;

    int mismatches = 0;

    for (int i = 0; i < FactoryPrograms::kParameterCount; ++i)
    {
        const auto& row = FactoryPrograms::kRows[(size_t) i];
        const juce::String key (row.presetKey);

        juce::var found;
        bool foundKey = false;

        for (auto& prop : obj->getProperties())
        {
            if (prop.name.toString().trim() == key)
            {
                found = prop.value;
                foundKey = true;
                break;
            }
        }

        if (! foundKey)
        {
            ++mismatches;
            continue;
        }

        auto* arr = found.getArray();

        if (arr == nullptr || arr->size() < 2)
        {
            ++mismatches;
            continue;
        }

        auto* values = arr->getReference (1).getArray();

        if (values == nullptr || values->size() != FactoryPrograms::kProgramCount)
        {
            ++mismatches;
            continue;
        }

        for (int program = 0; program < FactoryPrograms::kProgramCount; ++program)
            if ((int) values->getReference (program) != (int) row.values[(size_t) program])
                ++mismatches;
    }

    return mismatches;
}

void testFactoryTableMatchesJsonMirrors()
{
    std::printf ("factory table equals the on-disk JSON mirrors...\n");

    const juce::String root (PRA32_REPO_ROOT);

    const juce::File files[] = {
        juce::File (root).getChildFile ("pra32-u2-prog-factory-presets.json"),
        juce::File (root).getChildFile ("web_app").getChildFile ("data").getChildFile ("presets.json")
    };

    for (const auto& file : files)
    {
        if (! file.existsAsFile())
        {
            std::printf ("  WARNING: mirror not found: %s\n", file.getFullPathName().toRawUTF8());
            CHECK (false);
            continue;
        }

        const int m = compareJsonToTable (juce::JSON::parse (file.loadFileAsString()));
        std::printf ("  %s mismatches=%d\n", file.getFileName().toRawUTF8(), m);
        CHECK (m == 0);
    }
}

void testStartupPreset()
{
    std::printf ("startup agrees with factory program 0 everywhere...\n");

    PRA32ColorcoderAudioProcessor p;   // not prepared: state must already agree

    const auto& ps = SynthParameters::getParameters();
    int mismatches = 0;

    for (int i = 0; i < (int) ps.size(); ++i)
    {
        const int expected = FactoryPrograms::valueFor (i, 0);
        const int engine   = p.getEngineParameterValue (i);
        const int apvts    = (int) std::lround (p.getAPVTS().getRawParameterValue (ps[(size_t) i].id)->load());

        if (engine != expected) ++mismatches;
        if (apvts  != expected) ++mismatches;
    }

    std::printf ("  startup mismatches=%d preset=%d\n", mismatches, p.getCurrentFactoryPreset());

    CHECK (mismatches == 0);
    CHECK (p.getCurrentFactoryPreset() == 0);
}

void testProgramChangeEngineMatchesApvts()
{
    std::printf ("MIDI Program Change: engine and APVTS equal the factory table...\n");

    const int programs[] = { 0, 1, 7, 15, 16, 20, 25 };
    const auto& ps = SynthParameters::getParameters();

    for (int program : programs)
    {
        PRA32ColorcoderAudioProcessor p;
        p.prepareToPlay (48000.0, 512);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::programChange (1, program), 0);

        renderBlock (p, 512, midi);
        p.flushDeferredUpdates();

        int mismatches = 0;

        for (int i = 0; i < (int) ps.size(); ++i)
        {
            const int expected = FactoryPrograms::valueFor (i, program);
            const int engine   = p.getEngineParameterValue (i);
            const int apvts    = (int) std::lround (p.getAPVTS().getRawParameterValue (ps[(size_t) i].id)->load());

            if (engine != expected) ++mismatches;
            if (apvts  != expected) ++mismatches;
            if (engine != apvts)    ++mismatches;
        }

        std::printf ("  program %2d mismatches=%d\n", program, mismatches);
        CHECK (mismatches == 0);
        CHECK (p.getCurrentFactoryPreset() == program);
    }

    // Program-by-CC must reach the same authority.
    {
        PRA32ColorcoderAudioProcessor p;
        p.prepareToPlay (48000.0, 512);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 112 + 3, 0), 0);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 112 + 3, 127), 1);

        renderBlock (p, 512, midi);
        p.flushDeferredUpdates();

        int mismatches = 0;

        for (int i = 0; i < (int) ps.size(); ++i)
            if (p.getEngineParameterValue (i) != FactoryPrograms::valueFor (i, 3))
                ++mismatches;

        std::printf ("  PC-by-CC(3) mismatches=%d\n", mismatches);
        CHECK (mismatches == 0);
        CHECK (p.getCurrentFactoryPreset() == 3);
    }
}

// -----------------------------------------------------------------------------
// Causal SRC / sample-accurate MIDI through the processor
// -----------------------------------------------------------------------------
int firstAbove (const std::vector<float>& x, double threshold)
{
    for (int i = 0; i < (int) x.size(); ++i)
        if (std::abs (x[(size_t) i]) > threshold)
            return i;

    return -1;
}

void testLatencyReporting()
{
    std::printf ("reported latency / prepareToPlay updates...\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double fs : rates)
    {
        PRA32ColorcoderAudioProcessor p;
        p.prepareToPlay (fs, 512);

        const int latency = p.getLatencySamples();
        const double expected = (std::abs (fs - pra32::kEngineSampleRate) < 1.0e-3)
                                    ? 0.0
                                    : pra32::Resampler::kGroupDelayEngineSamples * fs
                                          / pra32::kEngineSampleRate;

        std::printf ("  %7.0f Hz : latency=%d samples (expected %.1f)\n", fs, latency, expected);

        CHECK (std::abs ((double) latency - expected) <= 1.0);

        // Re-preparing at another rate must refresh the reported latency.
        p.prepareToPlay (44100.0, 512);
        const int at44 = p.getLatencySamples();

        p.prepareToPlay (96000.0, 512);
        const int at96 = p.getLatencySamples();

        CHECK (at44 > 0);
        CHECK (at96 > at44);
    }
}

void testNoteOnTiming()
{
    std::printf ("Note On response = event sample + reported latency...\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int    eventSample = 2048;
    const int    numSamples  = 24576;

    for (double fs : rates)
    {
        PRA32ColorcoderAudioProcessor p;
        p.prepareToPlay (fs, numSamples);

        setSustainPatch (p, 120);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), eventSample);

        const auto out = renderBlock (p, numSamples, midi);

        const int latency = p.getLatencySamples();
        const double peak = [&]
        {
            double m = 0.0;
            for (float v : out) m = std::max (m, (double) std::abs (v));
            return m;
        }();

        // A small fraction of the steady amplitude: the causal step response
        // reaches this shortly before the group delay.
        const int onset = firstAbove (out, 0.05 * peak);
        const int expected = eventSample + latency;

        std::printf ("  %7.0f Hz : onset=%d  expected=%d  (latency=%d, peak=%.4f)\n",
                     fs, onset, expected, latency, peak);

        CHECK (peak > 1.0e-3);
        CHECK (onset >= eventSample);
        CHECK (std::abs (onset - expected) <= 6);
    }
}

double maxDifference (const std::vector<float>& a, const std::vector<float>& b,
                      int from, int to)
{
    double m = 0.0;
    const int n = (int) std::min (a.size(), b.size());

    for (int i = std::max (0, from); i < std::min (to, n); ++i)
        m = std::max (m, (double) std::abs (a[(size_t) i] - b[(size_t) i]));

    return m;
}

void testNoteOffAndControllerTiming()
{
    std::printf ("Note Off / CC are causal and take effect after the event...\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int    eventSample = 4096;
    const int    numSamples  = 16384;

    for (double fs : rates)
    {
        // --- Note Off --------------------------------------------------------
        {
            PRA32ColorcoderAudioProcessor a;
            PRA32ColorcoderAudioProcessor b;
            a.prepareToPlay (fs, numSamples); setSustainPatch (a, 120);
            b.prepareToPlay (fs, numSamples); setSustainPatch (b, 120);

            juce::MidiBuffer ma, mb;
            ma.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            ma.addEvent (juce::MidiMessage::noteOff (1, 60, (juce::uint8) 64), eventSample);
            mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

            const auto ya = renderBlock (a, numSamples, ma);
            const auto yb = renderBlock (b, numSamples, mb);

            const double before = maxDifference (ya, yb, 0, eventSample);
            const double after  = maxDifference (ya, yb, eventSample,
                                                 eventSample + a.getLatencySamples() + 4096);

            std::printf ("  %7.0f Hz Note Off : before=%.3e after=%.3e\n", fs, before, after);

            CHECK (before < 1.0e-6);
            CHECK (after > 1.0e-3);
        }

        // --- Controller (filter cutoff) -------------------------------------
        {
            PRA32ColorcoderAudioProcessor a;
            PRA32ColorcoderAudioProcessor b;
            a.prepareToPlay (fs, numSamples); setSustainPatch (a, 127);
            b.prepareToPlay (fs, numSamples); setSustainPatch (b, 127);

            juce::MidiBuffer ma, mb;
            ma.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            ma.addEvent (juce::MidiMessage::controllerEvent (1, 74, 0), eventSample);
            mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

            const auto ya = renderBlock (a, numSamples, ma);
            const auto yb = renderBlock (b, numSamples, mb);

            const double before = maxDifference (ya, yb, 0, eventSample);
            const double after  = maxDifference (ya, yb, eventSample,
                                                 eventSample + a.getLatencySamples() + 4096);

            std::printf ("  %7.0f Hz CC cutoff: before=%.3e after=%.3e\n", fs, before, after);

            CHECK (before < 1.0e-6);
            CHECK (after > 1.0e-3);

            // The engine (realtime path) already holds the CC value while the
            // APVTS mirror is still deferred, which is the intended split.
            CHECK (a.getEngineParameterValue (10) == 0);
            CHECK ((int) std::lround (a.getAPVTS().getRawParameterValue ("filterCutoff")->load()) == 127);

            a.flushDeferredUpdates();
            CHECK ((int) std::lround (a.getAPVTS().getRawParameterValue ("filterCutoff")->load()) == 0);
        }
    }
}

void testPitchBendCausality()
{
    std::printf ("Pitch Bend is causal and only affects post-event samples...\n");

    const double rates[] = { 44100.0, 96000.0 };
    const int    eventSample = 4096;
    const int    numSamples  = 16384;

    for (double fs : rates)
    {
        PRA32ColorcoderAudioProcessor a;
        PRA32ColorcoderAudioProcessor b;
        a.prepareToPlay (fs, numSamples); setSustainPatch (a, 120);
        b.prepareToPlay (fs, numSamples); setSustainPatch (b, 120);

        juce::MidiBuffer ma, mb;
        ma.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        ma.addEvent (juce::MidiMessage::pitchWheel (1, 16383), eventSample);
        mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        const auto ya = renderBlock (a, numSamples, ma);
        const auto yb = renderBlock (b, numSamples, mb);

        const double before = maxDifference (ya, yb, 0, eventSample);
        const double after  = maxDifference (ya, yb, eventSample + a.getLatencySamples() + 256,
                                             numSamples);

        std::printf ("  %7.0f Hz : before=%.3e after=%.3e\n", fs, before, after);

        CHECK (before < 1.0e-6);
        CHECK (after > 1.0e-3);
    }
}

void testCausalityThroughProcessor()
{
    std::printf ("no future engine state leaks before the MIDI event...\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int    eventSample = 4096;
    const int    numSamples  = 16384;

    for (double fs : rates)
    {
        PRA32ColorcoderAudioProcessor withEvent;
        withEvent.prepareToPlay (fs, numSamples);
        setSustainPatch (withEvent, 120);

        PRA32ColorcoderAudioProcessor withoutEvent;
        withoutEvent.prepareToPlay (fs, numSamples);
        setSustainPatch (withoutEvent, 120);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), eventSample);

        const auto a = renderBlock (withEvent, numSamples, midi);
        const auto b = renderBlock (withoutEvent, numSamples);

        double maxBefore = 0.0;
        for (int i = 0; i < eventSample; ++i)
            maxBefore = std::max (maxBefore, (double) std::abs (a[(size_t) i]));

        double maxDiffBefore = 0.0;
        for (int i = 0; i < eventSample; ++i)
            maxDiffBefore = std::max (maxDiffBefore,
                                      (double) std::abs (a[(size_t) i] - b[(size_t) i]));

        std::printf ("  %7.0f Hz : max before event=%.3e diff=%.3e\n",
                     fs, maxBefore, maxDiffBefore);

        CHECK (maxBefore < 1.0e-6);
        CHECK (maxDiffBefore < 1.0e-6);
    }
}

void testProgramChangeTiming96k()
{
    std::printf ("Program Change timing at 96 kHz (incl. program 16..25)...\n");

    const double fs = 96000.0;
    const int eventSample = 8192;
    const int numSamples  = 24576;

    // A sustained note is playing; switching from an open program (0) to the
    // percussive HARPSICHORD preset (25, sustain 0) must only take effect from
    // the event sample on, after the reported SRC latency.
    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (fs, numSamples);

    setSustainPatch (p, 127);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::programChange (1, 25), eventSample);

    const auto out = renderBlock (p, numSamples, midi);

    // Causality: nothing about program 25 may be reflected before the event.
    PRA32ColorcoderAudioProcessor reference;
    reference.prepareToPlay (fs, numSamples);
    setSustainPatch (reference, 127);
    juce::MidiBuffer refMidi;
    refMidi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);

    const auto ref = renderBlock (reference, numSamples, refMidi);

    double maxDiffBefore = 0.0;
    for (int i = 0; i < eventSample; ++i)
        maxDiffBefore = std::max (maxDiffBefore,
                                  (double) std::abs (out[(size_t) i] - ref[(size_t) i]));

    // After the event + latency + FIR settling the patch must actually change.
    const int latency = p.getLatencySamples();
    const int settle  = latency + (int) pra32::Resampler::kTaps;

    auto rmsWindow = [] (const std::vector<float>& x, int from, int to)
    {
        double s = 0.0;
        int n = 0;
        for (int i = from; i < to && i < (int) x.size(); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
        return n > 0 ? std::sqrt (s / n) : 0.0;
    };

    const double beforeRms = rmsWindow (out, eventSample - 2048, eventSample);
    const double afterRms  = rmsWindow (out, eventSample + settle, eventSample + settle + 4096);

    std::printf ("  causality diff=%.3e  rms before=%.5f after=%.5f\n",
                 maxDiffBefore, beforeRms, afterRms);

    CHECK (maxDiffBefore < 1.0e-6);
    CHECK (afterRms < beforeRms * 0.6);   // the percussive preset must be measurably different

    p.flushDeferredUpdates();
    CHECK (p.getCurrentFactoryPreset() == 25);
    CHECK (p.getEngineParameterValue (10) == FactoryPrograms::valueFor (10, 25));
}

void testOfflineReset()
{
    std::printf ("offline reset leaves no residual history...\n");

    pra32::Resampler r;
    r.prepare (96000.0);

    auto dc = [] { return pra32::Resampler::Sample { 1.0f, 1.0f }; };

    for (int i = 0; i < 8000; ++i)
        r.process (dc);

    r.reset();

    const float firstAfterReset = r.process (dc).l;

    std::printf ("  first sample after reset=%.6f\n", (double) firstAfterReset);
    CHECK (std::abs (firstAfterReset) < 1.0e-3);

    // The DC response ramps back up over the kernel length.
    float late = 0.0f;
    for (int i = 0; i < 1024; ++i)
        late = r.process (dc).l;

    CHECK (std::abs (late - 1.0f) < 1.0e-3);
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
    testFirstRenderAfterRestore (48000.0);
    testFirstRenderAfterRestore (44100.0);
    testFirstRenderAfterRestore (96000.0);

    testFactoryTableStructure();
    testFactoryTableMatchesJsonMirrors();
    testStartupPreset();
    testProgramChangeEngineMatchesApvts();

    testLatencyReporting();
    testNoteOnTiming();
    testNoteOffAndControllerTiming();
    testPitchBendCausality();
    testCausalityThroughProcessor();
    testProgramChangeTiming96k();
    testOfflineReset();

    std::printf ("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

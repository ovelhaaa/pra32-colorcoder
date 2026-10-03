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
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <new>
#include <thread>

#ifndef PRA32_REPO_ROOT
#define PRA32_REPO_ROOT "."
#endif

// Thread-local guard catches C++ heap use in the processor render path without
// counting unrelated message-thread work. Buffers/MIDI are prepared beforehand.
thread_local bool trackAudioAllocations = false;
thread_local size_t audioAllocations = 0;
void* operator new (std::size_t size)
{
    if (trackAudioAllocations) ++audioAllocations;
    if (void* pointer = std::malloc (size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size) { return ::operator new (size); }
void operator delete (void* pointer) noexcept { std::free (pointer); }
void operator delete[] (void* pointer) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t) noexcept { std::free (pointer); }

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
    CHECK (p.isCurrentPatchEdited());
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

    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

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

    constexpr double eventSeconds = 0.05;
    constexpr double duration = 0.3;
    PRA32ColorcoderAudioProcessor reference;
    reference.prepareToPlay (48000.0, 14400);
    setSustainPatch (reference, 120);
    juce::MidiBuffer refMidi;
    refMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 2400);
    const auto engine48 = renderBlock (reference, 14400, refMidi);
    auto peakOf = [] (const auto& x) { double peak = 0; for (float v : x) peak = std::max (peak, (double) std::abs (v)); return peak; };
    const int referenceOnset = firstAbove (engine48, 0.05 * peakOf (engine48));

    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        const int eventSample = (int) std::lround (eventSeconds * fs);
        const int numSamples = (int) std::lround (duration * fs);
        PRA32ColorcoderAudioProcessor p;
        p.prepareToPlay (fs, numSamples);
        setSustainPatch (p, 120);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), eventSample);
        const auto out = renderBlock (p, numSamples, midi);

        // Independent 48 kHz processor stream, through the same tested SRC:
        // onset includes the synth's free-running oscillator/envelope state,
        // rather than assuming its intrinsic response time is zero.
        pra32::Resampler src;
        src.prepare (fs);
        size_t index = 0;
        std::vector<float> expected ((size_t) numSamples);
        for (auto& v : expected)
            v = src.process ([&] { const float x = index < engine48.size() ? engine48[index++] : 0.0f; return pra32::Resampler::Sample { x, x }; }).l;
        const double peak = peakOf (out);
        const int onset = firstAbove (out, 0.05 * peak);
        const int expectedOnset = firstAbove (expected, 0.05 * peakOf (expected));
        // Quantisation bound: one engine tick and one host tick. This compares
        // like-for-like onset, with no arbitrary six-host-sample threshold.
        CHECK (std::abs (onset - expectedOnset) <= (int) std::ceil (fs / 48000.0) + 1);
        CHECK (peak > 1.0e-3);
        CHECK (onset >= eventSample);
        for (int i = 0; i < eventSample; ++i) CHECK (out[(size_t) i] == 0.0f);
        const double compensated = (onset - p.getLatencySamples()) / fs;
        const double referenceTime = referenceOnset / 48000.0;
        // Compensated onset may differ by one engine tick plus one host tick
        // (source/threshold quantisation and integer latency reporting).
        CHECK (std::abs (compensated - referenceTime) <= 1.0 / 48000.0 + 1.0 / fs);
        std::printf ("  %.0f Hz event=%d onset=%d oracle=%d latency=%d compensated-error=%.3f us\n",
            fs, eventSample, onset, expectedOnset, p.getLatencySamples(), (compensated-referenceTime)*1.0e6);
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

    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
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

    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
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

void testDirtyRecallAndInvalidState()
{
    std::printf ("factory/user dirty baselines and untrusted program indices...\n");
    PRA32ColorcoderAudioProcessor p;
    auto recall = [&] { juce::MemoryBlock data; p.getStateInformation (data); p.setStateInformation (data.getData(), (int) data.getSize()); };
    p.loadPreset (7); recall(); CHECK (! p.isCurrentPatchEdited());
    setParam (p, "filterCutoff", 1); recall(); CHECK (p.isCurrentPatchEdited());
    p.loadPreset (7); CHECK (! p.isCurrentPatchEdited());
    p.loadPresetFromJson ("{}"); recall(); CHECK (! p.isCurrentPatchEdited());
    setParam (p, "filterCutoff", 2); recall(); CHECK (p.isCurrentPatchEdited());
    p.loadPreset (0); CHECK (! p.isCurrentPatchEdited());
    setParam (p, "filterCutoff", 3); CHECK (p.isCurrentPatchEdited());
    for (auto index : { "-2", "26", "2147483647", "4294967296", "nan", "1.5", "oops", "" })
    {
        auto tree = p.getAPVTS().copyState();
        tree.setProperty ("uiPreset", index, nullptr);
        auto xml = tree.createXml();
        juce::MemoryBlock data;
        juce::AudioProcessor::copyXmlToBinary (*xml, data);
        p.setStateInformation (data.getData(), (int) data.getSize());
        CHECK (p.getCurrentFactoryPreset() == -1);
        CHECK ((int) p.getUiProperty ("uiPreset") == -1);
    }
}

void testDeferredStateSnapshots()
{
    std::printf ("state snapshots on a host worker preserve committed patch and deferred MIDI...\n");
    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (48000, 64); p.loadPreset (7);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (1, 8), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 99), 1);
    renderBlock (p, 64, midi);
    // A newer host edit beats the pending CC, without a message-loop flush.
    setParam (p, "filterCutoff", 2);
    const auto committed = snapshot (p);
    std::array<juce::MemoryBlock, 16> states;
    std::thread reader ([&] { for (auto& state : states) p.getStateInformation (state); });
    reader.join();
    CHECK (snapshot (p) == committed);
    CHECK (p.getCurrentFactoryPreset() == 7);
    PRA32ColorcoderAudioProcessor restored;
    for (auto& state : states)
    {
        restored.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (snapshot (restored) == committed);
        CHECK (restored.getCurrentFactoryPreset() == 7);
        CHECK (restored.isCurrentPatchEdited());
    }
    p.flushDeferredUpdates();
    CHECK (p.getCurrentFactoryPreset() == 8); // saving did not consume the PC
    CHECK ((int) p.getAPVTS().getRawParameterValue ("filterCutoff")->load() == 2);
    CHECK (p.isCurrentPatchEdited());
    p.loadPresetFromJson ("{}"); setParam (p, "filterCutoff", 3);
    std::thread userReader ([&] { p.getStateInformation (states[0]); }); userReader.join();
    restored.setStateInformation (states[0].getData(), (int) states[0].getSize());
    CHECK (restored.getCurrentFactoryPreset() == -1);
    CHECK (restored.isCurrentPatchEdited());
    setParam (restored, "filterCutoff", SynthParameters::getParameters()[10].def);
    CHECK (! restored.isCurrentPatchEdited());

    // No message loop runs here. Concurrent factory transactions must never
    // expose a mixed parameter column / preset index / baseline to a reader.
    p.loadPreset (0);
    std::atomic<bool> valid { true };
    std::atomic<bool> started { false };
    std::thread stressReader ([&]
    {
        started.store (true);
        for (int pass = 0; pass < 500; ++pass)
        {
            juce::MemoryBlock data; p.getStateInformation (data);
            auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), (int) data.getSize());
            if (! xml) { valid.store (false); continue; }
            const auto tree = juce::ValueTree::fromXml (*xml);
            const int program = (int) tree.getProperty ("uiPreset");
            const auto baseline = juce::JSON::parse (tree.getProperty ("patchBaseline").toString());
            const auto* values = baseline.getArray();
            const auto& params = SynthParameters::getParameters();
            if (! PRA32MidiState::isValidFactoryProgram (program) || ! values
                || values->size() != (int) params.size()) { valid.store (false); continue; }
            for (size_t i = 0; i < params.size(); ++i)
            {
                const int expected = FactoryPrograms::valueFor ((int) i, program);
                const int actual = (int) tree.getChildWithProperty ("id", params[i].id).getProperty ("value");
                if (actual != expected || (int) (*values)[(int) i] != expected) valid.store (false);
            }
        }
    });
    while (! started.load()) std::this_thread::yield();
    for (int pass = 0; pass < 500; ++pass) p.loadPreset (pass % p.getNumPrograms());
    stressReader.join(); CHECK (valid.load());
}

double signalRms (const std::vector<float>& signal);

void testKeyboardReprepare()
{
    std::printf ("keyboard queue, stale offs, overflow and normal input after reprepare...\n");
    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (48000, 64); setSustainPatch (p, 127);
    setParam (p, "relEqDcy", 0); setParam (p, "chorusMix", 0); setParam (p, "delayDepth", 0);
    p.keyboardState.noteOn (1, 60, 1.0f);
    p.prepareToPlay (48000, 64);
    CHECK (signalRms (renderBlock (p, 4096)) == 0);
    p.keyboardState.noteOff (1, 60, 0); // stale off queued before prepare
    p.prepareToPlay (48000, 64);
    juce::MidiBuffer fresh; fresh.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
    CHECK (signalRms (renderBlock (p, 4096, fresh)) > .01);
    // Overflow would panic and discard the next UI note if it survived.
    for (int i = 0; i < 300; ++i)
    {
        p.keyboardState.noteOn (1, 61, 1.0f);
        p.keyboardState.noteOff (1, 61, 0);
    }
    p.prepareToPlay (96000, 64);
    // Queue before the first block so a surviving overflow flag would discard it.
    p.keyboardState.noteOn (1, 62, 1.0f);
    CHECK (signalRms (renderBlock (p, 8192)) > .01);
    p.keyboardState.noteOff (1, 62, 0);
    renderBlock (p, 16000);
    CHECK (signalRms (renderBlock (p, 16000)) < 1.0e-4);
}

void testJsonValidation()
{
    std::printf ("JSON schema, legacy compatibility, transactional validation...\n");
    PRA32ColorcoderAudioProcessor p;
    p.loadPresetFromJson ("{\"FILTER_CUTOFF\": -10, \"FILTER_RESO\": 200, \"unknown\": false}");
    CHECK (snapshot (p)[10] == 0); CHECK (snapshot (p)[11] == 127);
    const auto& ps = SynthParameters::getParameters();
    for (size_t i = 0; i < ps.size(); ++i)
        if (i != 10 && i != 11) CHECK (snapshot (p)[i] == ps[i].def);
    p.loadPresetFromJson ("{\"FILTER_CUTOFF\": [[23]]}"); CHECK (snapshot (p)[10] == 23);
    p.loadPresetFromJson ("{\"schemaVersion\":1,\"parameters\":{\"FILTER_CUTOFF\":34}}"); CHECK (snapshot (p)[10] == 34);
    const auto before = snapshot (p);
    for (auto invalid : { "[1]", "null", "broken", "{", "{\"FILTER_CUTOFF\":\"12\"}", "{\"FILTER_CUTOFF\":true}",
                          "{\"FILTER_CUTOFF\":1e999}", "{\"schemaVersion\":2,\"parameters\":{}}", "{\"schemaVersion\":1,\"parameters\":[]}" })
    { p.loadPresetFromJson (invalid); CHECK (snapshot (p) == before); }
    p.loadPresetFromJson ("{}");
    for (size_t i = 0; i < ps.size(); ++i) CHECK (snapshot (p)[i] == ps[i].def);
}

juce::Component* findControl (juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren())
        if (auto* found = findControl (*child, id)) return found;
    return nullptr;
}

void testUiCoverageAndLifecycle()
{
    std::printf ("every APVTS parameter has a visible, parented, functional GUI control...\n");
    PRA32ColorcoderAudioProcessor p;
    const char* sections[] = { "OSC", "FILTER", "ENVS", "MOD", "FX", "CHARACTER" };
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        editor->setVisible (true);
        for (auto* child : editor->getChildren())
            if (auto* page = dynamic_cast<PRA32ParameterPage*> (child))
                page->setVisible (true);
        for (const auto& pd : SynthParameters::getParameters())
        {
            auto* control = findControl (*editor, pd.id);
            CHECK (control != nullptr);
            if (control == nullptr) { std::printf ("  missing %s\n", pd.id.toRawUTF8()); continue; }
            CHECK (control->getParentComponent() != nullptr);
            // Console smoke test has no native peer, so isShowing() is false
            // even for a valid visible hierarchy. Verify every ancestor instead.
            CHECK (editor->isParentOf (control));
            for (auto* node = control; node != nullptr; node = node->getParentComponent())
                CHECK (node->isVisible());
            CHECK (! control->getBounds().isEmpty());
            if (auto* slider = dynamic_cast<juce::Slider*> (control))
            {
                slider->setValue (pd.min, juce::sendNotificationSync);
                CHECK ((int) p.getAPVTS().getRawParameterValue (pd.id)->load() == pd.min);
                slider->setValue (pd.max, juce::sendNotificationSync);
                CHECK ((int) p.getAPVTS().getRawParameterValue (pd.id)->load() == pd.max);
            }
            else if (auto* button = dynamic_cast<juce::Button*> (control))
            {
                button->setToggleState (true, juce::sendNotificationSync);
                CHECK ((int) p.getAPVTS().getRawParameterValue (pd.id)->load() == pd.max);
                button->setToggleState (false, juce::sendNotificationSync);
                CHECK ((int) p.getAPVTS().getRawParameterValue (pd.id)->load() == pd.min);
            }
        }
        juce::ignoreUnused (sections);
        p.loadPreset (0);
    }
    // Actual active-page layout, including envelope knobs owned by EnvelopePanel.
    for (const auto* section : sections)
    {
        PRA32ParameterPage page (p, section);
        juce::Component parent; parent.addAndMakeVisible (page); parent.setVisible (true);
        page.setBounds (0, 0, 1000, 550);
        for (const auto& pd : SynthParameters::getParameters())
            if (pd.section == section)
            {
                auto* c = findControl (page, pd.id);
                CHECK (c != nullptr);
                if (c) { CHECK (c->isVisible()); CHECK (! c->getBounds().isEmpty()); CHECK (page.isParentOf (c)); }
            }
    }
}

void testAllParameterPaths()
{
    std::printf ("all host automation/CC/engine parameter paths...\n");
    PRA32ColorcoderAudioProcessor p;
    p.prepareToPlay (48000, 64);
    const auto& ps = SynthParameters::getParameters();
    for (size_t i = 0; i < ps.size(); ++i)
    {
        auto* param = p.getAPVTS().getParameter (ps[i].id);
        CHECK (param != nullptr && param->isAutomatable());
        for (int value : { ps[i].min, ps[i].max, ps[i].def })
        {
            setParam (p, ps[i].id, value); renderBlock (p, 64);
            CHECK (p.getEngineParameterValue ((int) i) == value);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, ps[i].cc, value), 17);
            renderBlock (p, 64, midi); p.flushDeferredUpdates();
            CHECK (p.getEngineParameterValue ((int) i) == value);
            CHECK ((int) p.getAPVTS().getRawParameterValue (ps[i].id)->load() == value);
        }
    }
}

std::vector<float> renderTimeline (PRA32ColorcoderAudioProcessor& p, int total, int blockSize, const juce::MidiBuffer& events)
{
    std::vector<float> output;
    for (int start = 0; start < total; start += blockSize)
    {
        const int n = std::min (blockSize, total - start);
        juce::MidiBuffer midi;
        midi.addEvents (events, start, n, -start);
        const auto block = renderBlock (p, n, midi);
        output.insert (output.end(), block.begin(), block.end());
        // Emulate the real message loop, so MIDI mirrors are block-independent.
        p.flushDeferredUpdates();
    }
    return output;
}

void testRateBlockLifecycleMatrix()
{
    std::printf ("full processor rates x block sizes / state / MIDI / prepare cycles...\n");
    for (double fs : { 44100., 48000., 88200., 96000., 176400., 192000. })
    {
        const int total = (int) (fs * 0.12);
        juce::MidiBuffer events;
        events.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), (int) (fs * .01));
        events.addEvent (juce::MidiMessage::programChange (1, 1), (int) (fs * .03));
        events.addEvent (juce::MidiMessage::controllerEvent (1, 74, 96), (int) (fs * .04));
        events.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 85), (int) (fs * .05));
        events.addEvent (juce::MidiMessage::pitchWheel (1, 12000), (int) (fs * .06));
        events.addEvent (juce::MidiMessage::noteOff (1, 60), (int) (fs * .08));
        PRA32ColorcoderAudioProcessor ref; ref.prepareToPlay (fs, total);
        const auto expected = renderTimeline (ref, total, total, events);
        for (int size : { 1, 16, 32, 64, 128, 256, 512, 1024, 2048 })
        {
            PRA32ColorcoderAudioProcessor p;
            juce::MemoryBlock state; p.getStateInformation (state);
            p.setStateInformation (state.getData(), (int) state.getSize());
            p.prepareToPlay (fs, size);
            const auto out = renderTimeline (p, total, size, events);
            CHECK (maxDifference (out, expected, 0, total) == 0.0);
            for (float value : out) CHECK (std::isfinite (value));
            p.releaseResources(); p.prepareToPlay (48000., 32); renderBlock (p, 32);
            p.releaseResources(); p.prepareToPlay (fs, size);
            p.setStateInformation (state.getData(), (int) state.getSize()); renderBlock (p, size);
        }
        std::printf ("  %.0f Hz: nine block sizes checked\n", fs);
    }
}

double signalRms (const std::vector<float>& signal)
{
    double sum = 0; for (float x : signal) sum += (double) x * x;
    return std::sqrt (sum / signal.size());
}

void testMidiPerformanceAndRecovery()
{
    std::printf ("MIDI sustain/repeated/overlap/modes/panic/pressure/breath/velocity...\n");
    for (int mode : { 0, 64, 96, 127 })
    {
        PRA32ColorcoderAudioProcessor p; p.prepareToPlay (48000, 512);
        setSustainPatch (p, 127); setParam (p, "relEqDcy", 0);
        setParam (p, "chorusMix", 0); setParam (p, "delayDepth", 0);
        juce::MidiBuffer notes;
        for (int note : { 60, 64, 67, 71, 72, 72 })
            notes.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        CHECK (signalRms (renderBlock (p, 4096, notes)) > .01);
        // Switch mode while notes are held, then All Notes Off must recover.
        setParam (p, "portaMode", mode); renderBlock (p, 512);
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (1), 37);
        renderBlock (p, 8000, off);
        CHECK (signalRms (renderBlock (p, 8000)) < 1.0e-4);
        CHECK (renderRms (p, 4096) > .01);
        juce::MidiBuffer panic; panic.addEvent (juce::MidiMessage::allSoundOff (1), 19);
        renderBlock (p, 8000, panic);
        CHECK (signalRms (renderBlock (p, 8000)) < 1.0e-4);
        CHECK (renderRms (p, 4096) > .01);
        juce::MidiBuffer pedal; pedal.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        pedal.addEvent (juce::MidiMessage::noteOff (1, 60), 1);
        CHECK (signalRms (renderBlock (p, 4096, pedal)) > .01);
        pedal.clear(); pedal.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        renderBlock (p, 8000, pedal);
        CHECK (signalRms (renderBlock (p, 8000)) < 1.0e-4);
    }
    for (int controller = 112; controller <= 119; ++controller)
    {
        PRA32ColorcoderAudioProcessor p; p.prepareToPlay (48000, 64);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::controllerEvent (1, controller, 0), 1);
        m.addEvent (juce::MidiMessage::controllerEvent (1, controller, 127), 32);
        renderBlock (p, 64, m); p.flushDeferredUpdates();
        CHECK (p.getCurrentFactoryPreset() == controller - 112);
    }
    for (int kind = 0; kind < 4; ++kind)
    {
        PRA32ColorcoderAudioProcessor a, b;
        for (auto* p : { &a, &b })
        {
            p->prepareToPlay (96000, 16000); setSustainPatch (*p, 100);
            setParam (*p, "aftTlfoAmt", 127); setParam (*p, "lfoDepth", 0);
            setParam (*p, "lfoOscAmt", 127); setParam (*p, "lfoRate", 110);
            setParam (*p, "bthAmpMod", kind == 2 ? 127 : 0);
            setParam (*p, "ampVelSens", kind == 3 ? 127 : 0);
        }
        juce::MidiBuffer ma, mb;
        ma.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) (kind == 3 ? 25 : 100)), 0);
        mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        if (kind == 0) ma.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 4096);
        if (kind == 1) ma.addEvent (juce::MidiMessage::aftertouchChange (1, 60, 127), 4096);
        if (kind == 2) ma.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 4096);
        const auto ya = renderBlock (a, 16000, ma), yb = renderBlock (b, 16000, mb);
        if (kind != 3) CHECK (maxDifference (ya, yb, 0, 4096) == 0);
        CHECK (maxDifference (ya, yb, 5000, 16000) > .001);
    }
    // UI keyboard queue reaches the engine without audio-thread MidiBuffer writes.
    PRA32ColorcoderAudioProcessor p; p.prepareToPlay (48000, 512);
    setSustainPatch (p, 127); setParam (p, "relEqDcy", 0);
    setParam (p, "chorusMix", 0); setParam (p, "delayDepth", 0);
    p.keyboardState.noteOn (1, 60, 1.0f);
    CHECK (signalRms (renderBlock (p, 4096)) > .01);
    p.keyboardState.noteOff (1, 60, 0);
    renderBlock (p, 8000); CHECK (signalRms (renderBlock (p, 8000)) < 1.0e-4);
}

void testMusicalStreamInvariance()
{
    std::printf ("full plugin pitch/envelope/LFO/delay stream equals resampled 48 kHz reference...\n");
    PRA32ColorcoderAudioProcessor ref;
    ref.prepareToPlay (48000, 512); ref.loadPreset (8);
    setParam (ref, "delayDepth", 100); setParam (ref, "delayFeedback", 100);
    setParam (ref, "lfoDepth", 80);
    juce::MemoryBlock state; ref.getStateInformation (state);
    juce::MidiBuffer midi; midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
    const auto source = renderTimeline (ref, 48000, 512, midi);
    for (double fs : { 44100., 48000., 88200., 96000., 176400., 192000. })
    {
        PRA32ColorcoderAudioProcessor p;
        p.setStateInformation (state.getData(), (int) state.getSize()); p.prepareToPlay (fs, 128);
        const auto actual = renderTimeline (p, (int) fs, 128, midi);
        pra32::Resampler r; r.prepare (fs); size_t i = 0;
        std::vector<float> expected (actual.size());
        for (auto& sample : expected)
            sample = r.process ([&] { const float x = source[i++]; return pra32::Resampler::Sample { x, x }; }).l;
        const double error = maxDifference (actual, expected, 0, (int) actual.size());
        std::printf ("  %.0f Hz: musical-stream max error %.3e\n", fs, error);
        CHECK (error == 0.0);
    }
}

void testResetAndRestorePendingMidi()
{
    std::printf ("prepare resets DSP history; state recall cancels pending MIDI overrides...\n");
    for (double fs : { 44100., 48000., 88200., 96000., 176400., 192000. })
    {
        PRA32ColorcoderAudioProcessor p;
        p.prepareToPlay (fs, 512); p.loadPreset (8);
        juce::MemoryBlock state; p.getStateInformation (state);
        renderRms (p, 8000);
        juce::MidiBuffer cc; cc.addEvent (juce::MidiMessage::controllerEvent (1, 74, 1), 0);
        renderBlock (p, 512, cc);
        p.setStateInformation (state.getData(), (int) state.getSize()); renderBlock (p, 1);
        CHECK (p.getEngineParameterValue (10) == FactoryPrograms::valueFor (10, 8));
        p.releaseResources(); p.prepareToPlay (fs, 16);
        PRA32ColorcoderAudioProcessor fresh;
        fresh.setStateInformation (state.getData(), (int) state.getSize()); fresh.prepareToPlay (fs, 16);
        CHECK (renderBlock (p, 4096) == renderBlock (fresh, 4096));
        CHECK (renderRms (p, 4096) == renderRms (fresh, 4096));
    }
}

void testRealtimeAllocationGuard()
{
    std::printf ("no C++ heap allocations in processBlock (MIDI, UI keyboard, SysEx)...\n");
    for (double fs : { 44100., 48000., 88200., 96000., 176400., 192000. })
    {
        PRA32ColorcoderAudioProcessor p; p.prepareToPlay (fs, 64);
        juce::AudioBuffer<float> buffer (2, 64);
        juce::MidiBuffer events;
        events.ensureSize (2048);
        events.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        events.addEvent (juce::MidiMessage::programChange (1, 25), 16);
        events.addEvent (juce::MidiMessage::controllerEvent (1, 74, 10), 32);
        events.addEvent (juce::MidiMessage::allSoundOff (1), 40);
        const std::array<uint8_t, 128> sysEx {};
        events.addEvent (juce::MidiMessage::createSysExMessage (sysEx.data(), (int) sysEx.size()), 48);
        p.keyboardState.noteOn (1, 64, .8f);
        audioAllocations = 0; trackAudioAllocations = true;
        p.processBlock (buffer, events);
        trackAudioAllocations = false;
        CHECK (audioAllocations == 0);
        std::printf ("  %.0f Hz allocations=%zu\n", fs, audioAllocations);
    }
}

void testPanicWithInfiniteRelease()
{
    std::printf ("All Sound Off terminates intentional infinite release and FX tails...\n");
    for (double fs : { 44100., 48000., 88200., 96000., 176400., 192000. })
    {
        PRA32ColorcoderAudioProcessor p; p.prepareToPlay (fs, 512);
        setSustainPatch (p, 127); setParam (p, "relEqDcy", 0);
        setParam (p, "ampRelease", 127); setParam (p, "delayDepth", 100);
        CHECK (renderRms (p, (int) (fs*.1)) > .01);
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allSoundOff (1), 0);
        renderBlock (p, (int) (fs*.02), off);
        CHECK (signalRms (renderBlock (p, (int) (fs*.1))) == 0.0);
        CHECK (renderRms (p, (int) (fs*.1)) > .01);
    }
}

void testTransportStopRecovery()
{
    struct PlayHead : juce::AudioPlayHead
    {
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        { PositionInfo info; info.setIsPlaying (playing); return info; }
    } playHead;
    std::printf ("host transport stop releases held notes and permits manual recovery...\n");
    PRA32ColorcoderAudioProcessor p; p.setPlayHead (&playHead); p.prepareToPlay (48000, 512);
    setSustainPatch (p, 127); setParam (p, "relEqDcy", 0);
    setParam (p, "chorusMix", 0); setParam (p, "delayDepth", 0);
    CHECK (renderRms (p, 4096) > .01);
    playHead.playing = false;
    renderBlock (p, 8000);
    CHECK (signalRms (renderBlock (p, 8000)) < 1.0e-4);
    CHECK (renderRms (p, 4096) > .01);
    p.setPlayHead (nullptr);
}

} // namespace

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("== PRA32-U2 plugin (JUCE) tests ==\n");

    testDeferredStateSnapshots();
    testKeyboardReprepare();
    testDirtyRecallAndInvalidState();
    testJsonValidation();
    testUiCoverageAndLifecycle();
    testAllParameterPaths();
    testRateBlockLifecycleMatrix();
    testMidiPerformanceAndRecovery();
    testMusicalStreamInvariance();
    testResetAndRestorePendingMidi();
    testRealtimeAllocationGuard();
    testPanicWithInfiniteRelease();
    testTransportStopRecovery();
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

#include "PluginProcessor.h"

#include <cmath>

// -----------------------------------------------------------------------------
// DUMMY HEADERS / WRAPPER DEFINITIONS FOR THE EMBEDDED ENGINE
// -----------------------------------------------------------------------------
#include "Arduino.h"
#include "EEPROM.h"
#include "I2S.h"

#ifndef PRA32_U2_USE_EMULATED_EEPROM
#define PRA32_U2_USE_EMULATED_EEPROM 1
#endif

// Provide dummy digitalWrite for JUCE environment
inline void digitalWrite(uint8_t pin, uint8_t val) {}

// Variables normally defined by the Arduino sketch / wrapper
EEPROMClass EEPROM;
I2SClass g_i2s_output;
uint8_t g_midi_ch = 0;

#include "pra32-u2-synth.h"

class PRA32Wrapper {
public:
    // The fourth template argument is SYNTH_ID, not a voice count. Voice count
    // comes from PRA32_U2_ENABLE_POLY_ON_1_CORE (set in CMakeLists.txt).
    PRA32_U2_Synth<false, false, false, 0> synth;
};
// -----------------------------------------------------------------------------

#include "FactoryProgramTable.h"

static_assert (FactoryPrograms::kProgramCount == PRA32MidiState::kFactoryProgramCount,
               "Factory program table and MIDI state must agree on the program count");

namespace
{
// Marks message-thread parameter writes as "self-inflicted" so the APVTS
// listener does not record them as a GUI/host take-over. Only the message
// thread performs these writes, but the flag is atomic because the listener can
// also run on the audio thread when the host automates a parameter.
struct DeferredWriteScope
{
    explicit DeferredWriteScope (std::atomic<bool>& flagToSet) : flag (flagToSet)
    {
        flag.store (true, std::memory_order_relaxed);
    }

    ~DeferredWriteScope()
    {
        flag.store (false, std::memory_order_relaxed);
    }

    std::atomic<bool>& flag;
};
} // namespace

//==============================================================================
const char* const PRA32ColorcoderAudioProcessor::factoryPresetNames[26] = {
    "INITIALIZATION", "SYNC LEAD", "SYNTH BRASS", "PLUCK SYNTH",
    "MONO SYNTH", "SYNTH BASS 1", "SYNTH BASS 2", "SYNTH BASS 3",
    "ETHEREAL PAD", "GRITTY BASS", "CHIPTUNE LEAD", "PERCUSSIVE PLUCK",
    "CLASSIC SWEEP", "DARK DRONE", "NOISE PERCUSSION", "BELL LEAD",
    "STRINGS ENSEMBLE", "ELECTRIC PIANO", "ACID BASS", "WARM PAD",
    "HORN SECTION", "MUSIC BOX", "WOBBLE BASS", "FLUTE",
    "GLASS PAD", "HARPSICHORD"
};

juce::String PRA32ColorcoderAudioProcessor::factoryPresetName (int index)
{
    return (index >= 0 && index < 26) ? factoryPresetNames[index] : juce::String();
}

juce::var PRA32ColorcoderAudioProcessor::getUiProperty (const juce::Identifier& key) const
{
    return apvts.state.getProperty (key);
}

void PRA32ColorcoderAudioProcessor::setUiProperty (const juce::Identifier& key,
                                                   const juce::var& value)
{
    apvts.state.setProperty (key, value, nullptr);
}

void PRA32ColorcoderAudioProcessor::capturePatchBaseline()
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    patchBaseline.clear();

    for (const auto& p : SynthParameters::getParameters())
        if (auto* value = apvts.getRawParameterValue (p.id))
            patchBaseline.push_back ((int) std::lround (value->load()));
}

bool PRA32ColorcoderAudioProcessor::isCurrentPatchEdited() const
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    const auto& params = SynthParameters::getParameters();

    if (patchBaseline.size() != params.size())
        return false;

    for (size_t i = 0; i < params.size(); ++i)
        if (auto* value = apvts.getRawParameterValue (params[i].id))
            if ((int) std::lround (value->load()) != patchBaseline[i])
                return true;

    return false;
}

//==============================================================================
PRA32ColorcoderAudioProcessor::PRA32ColorcoderAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", createParameterLayout()), synthWrapper(std::make_unique<PRA32Wrapper>())
{
    // Single authority for the startup state: factory preset 0 (INITIALIZATION).
    // The engine is initialised explicitly from the canonical factory table
    // (not from its own 16-program ROM), and the APVTS defaults are authored to
    // match column 0 of that same table, so engine, APVTS and UI agree from the
    // first sample. There is deliberately no transient last-program state.
    synthWrapper->synth.initialize();
    applyProgramToEngine (0);

    // Cache the atomic pointers for fast polling in the audio thread
    for (const auto& p : SynthParameters::getParameters()) {
        ParamBinding pb;
        pb.cc = p.cc;
        pb.valuePtr = apvts.getRawParameterValue(p.id);
        pb.lastValue = -1.0f; // Force update on first block
        paramBindings.push_back(pb);
    }

    parameterMailbox.clearAll();
    buildCcMap();
    capturePatchBaseline();

    // Track GUI/host edits so they can win against a stale MIDI CC or a delayed
    // preset mirror ("latest intentional event wins"). Our own writes are
    // wrapped in DeferredWriteScope so they are not mistaken for host edits.
    for (const auto& p : SynthParameters::getParameters())
        apvts.addParameterListener (p.id, this);

    // Drains audio-thread MIDI requests into the APVTS on the message thread.
    keyboardState.addListener (this);
    startTimer (15);
}

PRA32ColorcoderAudioProcessor::~PRA32ColorcoderAudioProcessor()
{
    stopTimer();
    keyboardState.removeListener (this);

    for (const auto& p : SynthParameters::getParameters())
        apvts.removeParameterListener (p.id, this);
}

void PRA32ColorcoderAudioProcessor::buildCcMap()
{
    ccToParamIndex.fill (-1);

    const auto& params = SynthParameters::getParameters();

    for (int i = 0; i < (int) params.size(); ++i)
    {
        const int cc = params[(size_t) i].cc;

        if (cc >= 0 && cc < (int) ccToParamIndex.size() && ccToParamIndex[(size_t) cc] < 0)
            ccToParamIndex[(size_t) cc] = i;
    }
}

juce::RangedAudioParameter*
PRA32ColorcoderAudioProcessor::parameterForIndex (int index) const
{
    const auto& params = SynthParameters::getParameters();

    if (index < 0 || index >= (int) params.size())
        return nullptr;

    return apvts.getParameter (params[(size_t) index].id);
}

int PRA32ColorcoderAudioProcessor::indexForParameterId (const juce::String& parameterID) const
{
    const auto& params = SynthParameters::getParameters();

    for (int i = 0; i < (int) params.size(); ++i)
        if (params[(size_t) i].id == parameterID)
            return i;

    return -1;
}

void PRA32ColorcoderAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    // Ignore the parameters this processor writes itself (preset mirror and
    // deferred flush). Anything else is a GUI/host edit and is stamped with a
    // fresh sequence so it beats every older MIDI/program event.
    if (applyDeferredInProgress.load (std::memory_order_relaxed))
        return;

    const int index = indexForParameterId (parameterID);

    if (index >= 0)
        parameterMailbox.markHostChange (index, sequences.next());
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PRA32ColorcoderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : SynthParameters::getParameters()) {
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{p.id, 1},
            p.label,
            p.min,
            p.max,
            p.def
        ));
    }
    return layout;
}

//==============================================================================
const juce::String PRA32ColorcoderAudioProcessor::getName() const
{
   #ifdef JucePlugin_Name
    return JucePlugin_Name;
   #else
    return "Tonecoder TC-32";
   #endif
}

bool PRA32ColorcoderAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PRA32ColorcoderAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PRA32ColorcoderAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PRA32ColorcoderAudioProcessor::getTailLengthSeconds() const
{
    // The instrument keeps sounding after Note Off: the amp/EG release decays,
    // the feedback delay repeats, and both can outlive a naive "0.0" report
    // (which makes hosts cut audible tails and can break offline/freeze).
    //
    // Conservative bound derived from the engine's own limits at 48 kHz:
    //   * delay: buffer 16384 samples, longest effective time ~16320 samples
    //     (~0.34 s), max feedback coefficient 127/256 (~0.496). Reaching -60 dB
    //     takes ~10 repeats -> ~3.4 s.
    //   * envelope release: the steepest non-infinite release coefficient
    //     (CC 126) has a ~2.5 s time constant -> ~17 s to -60 dB. CC 127 maps to
    //     an exactly non-decaying coefficient (an intentional "hold"), which no
    //     finite value can represent.
    // 20 s covers the audible portion of every bounded setting with margin.
    // This is intentionally a conservative constant rather than a hot-path
    // computation: it is stable, allocation-free and cheap to query.
    return 20.0;
}

int PRA32ColorcoderAudioProcessor::getNumPrograms()
{
    return PRA32MidiState::kFactoryProgramCount;
}

int PRA32ColorcoderAudioProcessor::getCurrentProgram()
{
    return juce::jmax (0, juce::jmin (PRA32MidiState::kFactoryProgramCount - 1, currentFactoryPreset.load()));
}

void PRA32ColorcoderAudioProcessor::setCurrentProgram (int index)
{
    if (PRA32MidiState::isValidFactoryProgram (index))
        loadPreset (index);
}

const juce::String PRA32ColorcoderAudioProcessor::getProgramName (int index)
{
    return factoryPresetName (index);
}

void PRA32ColorcoderAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName); // factory programs are read-only
}

void PRA32ColorcoderAudioProcessor::loadPreset(int index)
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    if (! PRA32MidiState::isValidFactoryProgram (index))
        return;

    // A preset takes ownership of every parameter: drop in-flight MIDI-CC
    // writes and any queued MIDI program change, then mirror the program into
    // the APVTS/host/UI. The engine picks the new values up at the next block
    // through updateEngineFromParameters().
    discardPendingParameterUpdates();
    clearPendingProgram();
    mirrorProgramToAPVTSAndUI (index);
}

void PRA32ColorcoderAudioProcessor::writeProgramValuesToAPVTS (int program)
{
    if (! PRA32MidiState::isValidFactoryProgram (program))
        return;

    const auto& params = SynthParameters::getParameters();

    for (size_t i = 0; i < params.size() && i < (size_t) FactoryPrograms::kParameterCount; ++i)
        if (auto* param = apvts.getParameter (params[i].id))
            param->setValueNotifyingHost (
                param->convertTo0to1 ((float) FactoryPrograms::valueFor ((int) i, program)));
}

void PRA32ColorcoderAudioProcessor::captureBaselineFromProgram (int program)
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    patchBaseline.clear();

    if (! PRA32MidiState::isValidFactoryProgram (program))
        return;

    const auto& params = SynthParameters::getParameters();

    for (size_t i = 0; i < params.size(); ++i)
        patchBaseline.push_back (i < (size_t) FactoryPrograms::kParameterCount
                                     ? FactoryPrograms::valueFor ((int) i, program)
                                     : params[i].def);
}

void PRA32ColorcoderAudioProcessor::finishProgramBookkeeping (int program)
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    // The baseline is the pure factory column, so any host/GUI value that won
    // after the program change is correctly reported as an edit.
    captureBaselineFromProgram (program);
    currentFactoryPreset = program;
    setUiProperty ("uiPreset", currentFactoryPreset.load());
    sendChangeMessage();
}

void PRA32ColorcoderAudioProcessor::mirrorProgramToAPVTSAndUI (int program)
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    if (! PRA32MidiState::isValidFactoryProgram (program))
        return;

    const DeferredWriteScope scope (applyDeferredInProgress);

    writeProgramValuesToAPVTS (program);
    finishProgramBookkeeping (program);
}

void PRA32ColorcoderAudioProcessor::applyProgramToEngine (int program) noexcept
{
    // Realtime path: the engine is mutated immediately, at the MIDI event sample.
    // Every parameter of the canonical table is pushed through control_change()
    // exactly as the block-boundary APVTS mirror would push it, so the engine
    // ends up with the same patch that later appears in the APVTS/UI. Programs
    // 16..25 are ordinary table columns, so they are as sample-accurate as 0..15.
    if (! PRA32MidiState::isValidFactoryProgram (program))
        return;

    for (int i = 0; i < FactoryPrograms::kParameterCount; ++i)
    {
        const auto& row = FactoryPrograms::kRows[(size_t) i];
        synthWrapper->synth.control_change ((uint8_t) row.cc,
                                            (uint8_t) row.values[(size_t) program]);
    }
}

int PRA32ColorcoderAudioProcessor::getEngineParameterValue (int parameterIndex)
{
    if (parameterIndex < 0 || parameterIndex >= (int) paramBindings.size())
        return -1;

    const int cc = paramBindings[(size_t) parameterIndex].cc;

    if (cc < 0 || cc >= 128)
        return -1;

    return (int) synthWrapper->synth.current_controller_value ((uint8_t) cc);
}

void PRA32ColorcoderAudioProcessor::queueProgramChange (int program) noexcept
{
    // Realtime path: stamp the deferred mirror with a fresh sequence.
    pendingProgram.publish (program, sequences.next());
}

void PRA32ColorcoderAudioProcessor::clearPendingProgram() noexcept
{
    pendingProgram.clear();
}

void PRA32ColorcoderAudioProcessor::applyParameterValue (int index, int value)
{
    if (auto* param = parameterForIndex (index))
        param->setValueNotifyingHost (
            param->convertTo0to1 ((float) juce::jlimit (0, 127, value)));
}

void PRA32ColorcoderAudioProcessor::loadPresetFromJson(const juce::String& jsonString)
{
    juce::var root;
    if (juce::JSON::parse (jsonString, root).failed() || ! root.isObject())
        return;

    auto* obj = root.getDynamicObject();
    if (obj == nullptr) return; // JUCE arrays also report isObject().
    const bool versioned = obj->hasProperty ("schemaVersion");
    if (versioned)
    {
        const auto version = obj->getProperty ("schemaVersion");
        if (! version.isInt() || (int) version != 1)
            return;
        obj = obj->getProperty ("parameters").getDynamicObject();
        if (obj == nullptr)
            return;
    }

    // Validate the entire document before committing. Full patch: absent
    // parameters use INITIALIZATION defaults, independent of the previous patch.
    std::vector<int> values;
    for (const auto& p : SynthParameters::getParameters())
    {
        juce::var value;
        bool found = false;
        for (const auto& prop : obj->getProperties())
            if (prop.name.toString().trim() == p.presetKey)
            {
                value = prop.value;
                found = true;
                break;
            }

        // Legacy patches also accept [value] and [[value]] containers.
        if (! versioned)
            for (int depth = 0; depth < 2 && value.isArray(); ++depth)
            {
                const auto* array = value.getArray();
                if (array->isEmpty()) return;
                const auto first = array->getReference (0);
                value = first;
            }

        int next = p.def;
        if (found)
        {
            if (! (value.isInt() || value.isInt64() || value.isDouble())) return;
            const double number = (double) value;
            if (! std::isfinite (number)) return;
            next = (int) std::lround (juce::jlimit ((double) p.min, (double) p.max, number));
        }
        values.push_back (next);
    }

    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    discardPendingParameterUpdates();
    clearPendingProgram();
    const DeferredWriteScope scope (applyDeferredInProgress);
    const auto& params = SynthParameters::getParameters();
    for (size_t i = 0; i < params.size(); ++i)
        if (auto* param = apvts.getParameter (params[i].id))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) values[i]));

    capturePatchBaseline();
    currentFactoryPreset = -1;
    setUiProperty ("uiPreset", -1);
    sendChangeMessage();
}

juce::String PRA32ColorcoderAudioProcessor::savePresetToJson()
{
    juce::DynamicObject::Ptr parameters = new juce::DynamicObject();
    for (const auto& p : SynthParameters::getParameters())
        parameters->setProperty (p.presetKey,
            (int) std::lround (apvts.getRawParameterValue (p.id)->load()));
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("schemaVersion", 1);
    root->setProperty ("parameters", juce::var (parameters.get()));
    return juce::JSON::toString (juce::var (root.get()));
}

//==============================================================================
void PRA32ColorcoderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // The PRA32 engine operates at a fixed 48 kHz. The resampler adapts its
    // output to the host rate and is a bit-transparent passthrough at 48 kHz.
    // All filter/history memory is reserved here (never on the audio thread).
    // Hosts call prepare with rendering stopped. Recreate the engine here to
    // reset held notes, envelopes and FX histories, while preserving the APVTS patch.
    {
        // Rendering is stopped here. Exclude the UI producer while resetting
        // the FIFO storage; the audio consumer never takes this lock.
        const juce::ScopedLock producerLock (keyboardProducerLock);
        keyboardFifo.reset();
        keyboardEvents.fill ({});
        keyboardOverflow.store (false);
        for (auto& note : hostKeyboardNotes) note.store (0);
    }
    synthWrapper = std::make_unique<PRA32Wrapper>();
    synthWrapper->synth.initialize();
    for (auto& binding : paramBindings)
    {
        binding.lastValue = -1.0f;
        binding.midiPending = false;
    }
    pcByCcValues.fill (0);
    wasTransportPlaying = false;
    updateEngineFromParameters();
    resampler.prepare (sampleRate);

    // The resampler is causal: it only ever requests engine samples at or before
    // the one that maps to the host sample being produced. Its windowed-sinc
    // prototype has a group delay of (kTaps - 1) / 2 engine samples, which is the
    // only latency it adds. Report it in host samples so the host can compensate.
    // The 48 kHz fast path is bit-transparent and reports exactly zero.
    setLatencySamples ((int) std::lround (resampler.getLatencyInHostSamples()));
}

void PRA32ColorcoderAudioProcessor::releaseResources()
{
    synthWrapper->synth.all_notes_off();
    resampler.reset();
}

bool PRA32ColorcoderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #endif

    return true;
  #endif
}

void PRA32ColorcoderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // Release held notes once on a host transport stop; stopped hosts may still
    // play manual MIDI/keyboard notes. Preserve normal release/FX tails.
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
        {
            const bool playing = position->getIsPlaying();
            if (wasTransportPlaying && ! playing)
            {
                synthWrapper->synth.all_notes_off();
                for (auto& note : hostKeyboardNotes) note.store (-1);
            }
            wasTransportPlaying = playing;
        }

    // 1. Apply block-boundary parameter changes (GUI, host automation, state
    //    recall, preset loads) to the engine.
    updateEngineFromParameters();

    // 2. Drain bounded UI events at the start of the next nonempty block.
    if (numSamples > 0)
    {
        const bool overflow = keyboardOverflow.exchange (false);
        if (overflow)
        {
            synthWrapper->synth.all_sound_off();
            for (auto& note : hostKeyboardNotes) note.store (-1);
        }
        int start1, size1, start2, size2;
        keyboardFifo.prepareToRead (keyboardFifo.getNumReady(), start1, size1, start2, size2);
        auto apply = [this] (int index)
        {
            const auto& event = keyboardEvents[(size_t) index];
            if (event.on) synthWrapper->synth.note_on ((uint8_t) event.note, (uint8_t) event.velocity);
            else synthWrapper->synth.note_off ((uint8_t) event.note);
        };
        if (! overflow)
        {
            for (int i = 0; i < size1; ++i) apply (start1 + i);
            for (int i = 0; i < size2; ++i) apply (start2 + i);
        }
        keyboardFifo.finishedRead (size1 + size2);
    }

    // 3. Render sample-accurately: audio up to each event, apply the event,
    //    then continue. The resampler keeps its phase/state across segments.
    PRA32MidiState::dispatchSampleAccurate (
        numSamples, midiMessages,
        [this, &buffer] (int start, int count) { renderAudioRange (buffer, start, count); },
        [this] (const juce::MidiMessage& message) { handleMidiMessage (message); });
}

void PRA32ColorcoderAudioProcessor::renderAudioRange (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;

    const int totalNumOutputChannels = getTotalNumOutputChannels();
    float* channelDataL = buffer.getWritePointer (0);
    float* channelDataR = (totalNumOutputChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    // Pull one engine sample at the fixed 48 kHz engine rate. The resampler asks
    // for as many as the current host ratio requires, so this lambda is only
    // invoked when a new engine sample is actually needed.
    auto pullEngineSample = [this]() -> pra32::Resampler::Sample
    {
        int16_t right_out = 0;
        int16_t left_out = synthWrapper->synth.process (0, 0, right_out);
        return { left_out / 32768.0f, right_out / 32768.0f };
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const auto out = resampler.process (pullEngineSample);

        channelDataL[startSample + i] = out.l;

        if (channelDataR != nullptr)
            channelDataR[startSample + i] = out.r;
    }
}

void PRA32ColorcoderAudioProcessor::updateEngineFromParameters()
{
    const auto generation = parameterResetGeneration.load (std::memory_order_acquire);
    if (generation != audioParameterResetGeneration)
    {
        for (auto& binding : paramBindings)
        {
            binding.midiPending = false;
            binding.lastValue = -1.0f;
        }
        audioParameterResetGeneration = generation;
    }
    const int count = juce::jmin ((int) paramBindings.size(), (int) ccToParamIndex.size());

    for (int i = 0; i < count; ++i)
    {
        auto& pb = paramBindings[(size_t) i];

        if (pb.valuePtr == nullptr)
            continue;

        const float val = pb.valuePtr->load (std::memory_order_relaxed);

        if (pb.midiPending)
        {
            const int apvtsValue = (int) std::lround (val);

            // A GUI/host edit recorded after the MIDI CC owns the parameter, so
            // stop waiting for the APVTS to catch up with the stale MIDI value.
            const bool hostTookOver =
                parameterMailbox.hostSequence (i) > pb.midiSequence;

            if (apvtsValue == pb.midiTarget)
            {
                // The APVTS caught up with the MIDI CC; adopt it and resume
                // normal host/GUI tracking.
                pb.midiPending = false;
                pb.lastValue = val;
            }
            else if (hostTookOver || --pb.midiPendingBlocks <= 0)
            {
                // The user/host changed the value while the MIDI update was in
                // flight, or the deferred write never landed; the APVTS wins.
                pb.midiPending = false;
                pb.lastValue = val;
                synthWrapper->synth.control_change (pb.cc, (uint8_t) juce::jlimit (0, 127, apvtsValue));
            }

            continue;
        }

        if (val != pb.lastValue)
        {
            pb.lastValue = val;
            synthWrapper->synth.control_change (pb.cc, (uint8_t) juce::jlimit (0, 127, (int) std::lround (val)));
        }
    }
}

void PRA32ColorcoderAudioProcessor::handleMidiMessage (const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        synthWrapper->synth.note_on (msg.getNoteNumber(), msg.getVelocity());
        hostKeyboardNotes[(size_t) msg.getNoteNumber()].store (1);
    }
    else if (msg.isNoteOff())
    {
        synthWrapper->synth.note_off (msg.getNoteNumber());
        hostKeyboardNotes[(size_t) msg.getNoteNumber()].store (-1);
    }
    else if (msg.isController())
    {
        const int controller = msg.getControllerNumber();
        const int value = msg.getControllerValue();
        if (controller == 120 || controller == 123)
            for (auto& note : hostKeyboardNotes) note.store (-1);

        // --- Program Change by CC (CC112..CC119) -----------------------------
        // The engine's internal program_change() ROM is not the plugin's preset
        // authority, so these controllers are not forwarded to the engine.
        // Instead the 0->1 gate is detected here and the canonical factory table
        // is applied through exactly the same engine path as a MIDI Program
        // Change. The deferred APVTS/UI/browser sync is enqueued as before.
        const int pcByCcIndex = PRA32MidiState::pcByCcProgramIndex (controller);

        if (pcByCcIndex >= 0)
        {
            const int previousValue = pcByCcValues[(size_t) pcByCcIndex];
            pcByCcValues[(size_t) pcByCcIndex] = value;

            if (PRA32MidiState::pcByCcTriggers (previousValue, value))
            {
                applyProgramToEngine (pcByCcIndex);
                queueProgramChange (pcByCcIndex);
            }

            return;
        }

        // Embedded EEPROM/program-writing commands have no meaning in a
        // desktop factory/JSON patch workflow. Never execute them in rendering.
        if (controller == PROG_N_TO_W_TO || controller == WRITE_P_TO_PROG)
            return;

        // Always forward to the engine immediately so performance controllers
        // (sustain, expression, breath, modulation) stay sample-accurate.
        synthWrapper->synth.control_change ((uint8_t) controller, (uint8_t) value);

        // If this CC is also an automatable parameter, mirror it into the APVTS
        // through a lock-free request so GUI/host/session state stay coherent.
        if (controller >= 0 && controller < (int) ccToParamIndex.size())
        {
            const int parameterIndex = ccToParamIndex[(size_t) controller];

            if (parameterIndex >= 0 && parameterIndex < (int) paramBindings.size())
            {
                auto& pb = paramBindings[(size_t) parameterIndex];

                if (! pb.midiPending)
                    pb.preMidiValue = pb.lastValue;

                // Stamp this CC into the shared timeline. The message thread will
                // only mirror it if no newer host/GUI or program event exists.
                const auto sequence = sequences.next();

                pb.midiPending = true;
                pb.midiTarget = value;
                pb.midiPendingBlocks = 20;
                pb.lastValue = (float) value;
                pb.midiSequence = sequence;

                parameterMailbox.publish (parameterIndex, value,
                                          (int) std::lround (pb.preMidiValue),
                                          sequence);
            }
        }
    }
    else if (msg.isPitchWheel())
    {
        // JUCE pitch wheel is 0-16383, centre 8192.
        const int value = msg.getPitchWheelValue();
        synthWrapper->synth.pitch_bend ((uint8_t) (value & 0x7F), (uint8_t) ((value >> 7) & 0x7F));
    }
    else if (msg.isChannelPressure())
    {
        synthWrapper->synth.after_touch_channel ((uint8_t) msg.getChannelPressureValue());
    }
    else if (msg.isAftertouch())
    {
        synthWrapper->synth.after_touch_poly ((uint8_t) msg.getNoteNumber(),
                                              (uint8_t) msg.getAfterTouchValue());
    }
    else if (msg.isProgramChange())
    {
        const int program = msg.getProgramChangeNumber();

        // Sample-accurate: the engine switches at this exact event sample. The
        // APVTS/UI/host follow asynchronously through the deferred sync. Invalid
        // programs (>= kFactoryProgramCount) are ignored.
        if (PRA32MidiState::isValidFactoryProgram (program))
        {
            applyProgramToEngine (program);
            queueProgramChange (program);
        }
    }
}

void PRA32ColorcoderAudioProcessor::discardPendingParameterUpdates() noexcept
{
    parameterMailbox.clearAll();
    // Cancel audio-owned CC overrides at its next block, without racing the
    // ParamBinding fields from the state/preset-loading thread.
    parameterResetGeneration.fetch_add (1, std::memory_order_release);
}

void PRA32ColorcoderAudioProcessor::flushDeferredUpdates()
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    // Unified deferred timeline: a Program Change is a global event, a MIDI CC
    // is a per-parameter event and a GUI/host edit is a per-parameter event.
    // They all carry a sequence from the same generator, so for each parameter
    // we simply apply the one with the highest sequence ("latest intentional
    // event wins"). This is what lets "Program Change -> CC" keep the CC while
    // "CC -> Program Change" lets the preset win.
    const auto program = pendingProgram.peek();
    const bool hasProgram = program.sequence != PRA32MidiState::kNoSequence
                            && PRA32MidiState::isValidFactoryProgram (program.program);

    const int count = (int) paramBindings.size();

    {
        const DeferredWriteScope scope (applyDeferredInProgress);

        for (int i = 0; i < count; ++i)
        {
            auto& pb = paramBindings[(size_t) i];

            const int currentValue = (pb.valuePtr != nullptr)
                                         ? (int) std::lround (pb.valuePtr->load (std::memory_order_relaxed))
                                         : -1;

            const auto obs = parameterMailbox.observe (i);

            const int programValue =
                (hasProgram && i < FactoryPrograms::kParameterCount)
                    ? FactoryPrograms::valueFor (i, program.program)
                    : -1;

            const auto resolution = PRA32MidiState::resolveDeferredParameter (
                obs, hasProgram ? program.sequence : PRA32MidiState::kNoSequence,
                programValue);

            if (resolution.source == PRA32MidiState::ParamEventSource::Midi)
            {
                // Remove exactly the publication we read; a newer publication
                // survives the failed compare-exchange and is handled next tick.
                if (parameterMailbox.tryConsume (i, obs))
                {
                    // A host/GUI edit may have raced in after we sampled its
                    // watermark; re-check before overwriting the APVTS value.
                    if (parameterMailbox.hostSequence (i) <= obs.sequence
                        && currentValue != resolution.value)
                    {
                        applyParameterValue (i, resolution.value);
                    }
                }
            }
            else
            {
                // Host or Program wins: drop any stale MIDI publication.
                if (resolution.shouldConsumeMidi)
                    parameterMailbox.tryConsume (i, obs);

                if (resolution.source == PRA32MidiState::ParamEventSource::Program
                    && currentValue != resolution.value)
                {
                    applyParameterValue (i, resolution.value);
                }
            }
        }
    }

    if (program.sequence != PRA32MidiState::kNoSequence && ! hasProgram)
        pendingProgram.clearIfUnchanged (program);

    // Only finish the bookkeeping if no newer Program Change was published while
    // the mirror was in flight; otherwise leave it for the next tick.
    if (hasProgram && pendingProgram.clearIfUnchanged (program))
        finishProgramBookkeeping (program.program);
}

void PRA32ColorcoderAudioProcessor::queueKeyboardEvent (int note, int velocity, bool on)
{
    const juce::ScopedLock producerLock (keyboardProducerLock);
    if (mirroringKeyboard) return;
    int start1, size1, start2, size2;
    keyboardFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 + size2 == 0) { keyboardOverflow.store (true); return; }
    keyboardEvents[(size_t) (size1 > 0 ? start1 : start2)] = { note, velocity, on };
    keyboardFifo.finishedWrite (1);
}

void PRA32ColorcoderAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity)
{
    queueKeyboardEvent (note, juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f)), true);
}

void PRA32ColorcoderAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int, int note, float)
{
    queueKeyboardEvent (note, 0, false);
}

void PRA32ColorcoderAudioProcessor::timerCallback()
{
    flushDeferredUpdates();
    // JUCE's keyboard lock, listeners and indirect-event cleanup stay on the UI thread.
    mirroringKeyboard = true;
    for (int note = 0; note < 128; ++note)
    {
        const int change = hostKeyboardNotes[(size_t) note].exchange (0);
        if (change > 0) keyboardState.noteOn (1, note, 1.0f);
        if (change < 0) keyboardState.noteOff (1, note, 0.0f);
    }
    juce::MidiBuffer unused;
    keyboardState.processNextMidiBuffer (unused, 0, 0, false);
    mirroringKeyboard = false;
}

#include "PluginEditor.h"

//==============================================================================
bool PRA32ColorcoderAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PRA32ColorcoderAudioProcessor::createEditor()
{
    return new PRA32ColorcoderAudioProcessorEditor (*this);
}

//==============================================================================
void PRA32ColorcoderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save the committed APVTS patch. Pending MIDI remains deferred until the
    // message-thread timer runs; saving must not notify the host or mutate UI.
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    auto state = apvts.copyState();
    state.setProperty ("uiPreset", currentFactoryPreset.load(), nullptr);
    juce::Array<juce::var> baseline;
    for (int value : patchBaseline) baseline.add (value);
    state.setProperty ("patchBaseline", juce::JSON::toString (juce::var (baseline), true), nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PRA32ColorcoderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const juce::ScopedLock snapshotLock (stateSnapshotLock);
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            discardPendingParameterUpdates();
            clearPendingProgram();

            auto restored = juce::ValueTree::fromXml (*xmlState);
            // XML attributes arrive as strings. Parse strictly, including range
            // and integrality, before using an untrusted program index.
            const auto presetText = restored.getProperty ("uiPreset").toString();
            int program = -1;
            for (int i = 0; i < getNumPrograms(); ++i)
                if (presetText == juce::String (i)) program = i;
            restored.setProperty ("uiPreset", program, nullptr);

            for (const auto& pd : SynthParameters::getParameters())
            {
                auto child = restored.getChildWithProperty ("id", pd.id);
                if (! child.isValid())
                {
                    child = juce::ValueTree ("PARAM");
                    child.setProperty ("id", pd.id, nullptr);
                    restored.addChild (child, -1, nullptr);
                    child.setProperty ("value", pd.def, nullptr);
                }
                const auto raw = child.getProperty ("value");
                const double number = (double) raw;
                child.setProperty ("value", std::isfinite (number)
                    ? juce::jlimit ((double) pd.min, (double) pd.max, number)
                    : (double) pd.def, nullptr);
            }
            {
                const DeferredWriteScope scope (applyDeferredInProgress);
                apvts.replaceState (restored);
            }

            currentFactoryPreset = program;
            if (program >= 0)
                captureBaselineFromProgram (program);
            else
            {
                // USER baseline survives session recall; legacy sessions without
                // baseline metadata adopt the restored patch as their baseline.
                capturePatchBaseline();
                const auto saved = juce::JSON::parse (restored.getProperty ("patchBaseline").toString());
                if (const auto* array = saved.getArray())
                    if (array->size() == (int) patchBaseline.size())
                    {
                        bool valid = true;
                        const auto& ps = SynthParameters::getParameters();
                        for (int i = 0; i < array->size(); ++i)
                        {
                            const auto& v = array->getReference (i);
                            valid = valid && v.isInt() && (int) v >= ps[(size_t) i].min
                                                       && (int) v <= ps[(size_t) i].max;
                        }
                        if (valid)
                            for (int i = 0; i < array->size(); ++i)
                                patchBaseline[(size_t) i] = (int) array->getReference (i);
                    }
            }
            sendChangeMessage();
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PRA32ColorcoderAudioProcessor();
}

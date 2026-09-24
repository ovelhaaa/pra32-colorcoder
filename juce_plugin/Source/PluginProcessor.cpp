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

#include "FactoryPresets.h"

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
const char* const PRA32ColorcoderAudioProcessor::factoryPresetNames[16] = {
    "INITIALIZATION", "SYNC LEAD", "SYNTH BRASS", "PLUCK SYNTH",
    "MONO SYNTH", "SYNTH BASS 1", "SYNTH BASS 2", "SYNTH BASS 3",
    "ETHEREAL PAD", "GRITTY BASS", "CHIPTUNE LEAD", "PERCUSSIVE PLUCK",
    "CLASSIC SWEEP", "DARK DRONE", "NOISE PERCUSSION", "BELL LEAD"
};

juce::String PRA32ColorcoderAudioProcessor::factoryPresetName (int index)
{
    return (index >= 0 && index < 16) ? factoryPresetNames[index] : juce::String();
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
    patchBaseline.clear();

    for (const auto& p : SynthParameters::getParameters())
        if (auto* value = apvts.getRawParameterValue (p.id))
            patchBaseline.push_back ((int) std::lround (value->load()));
}

bool PRA32ColorcoderAudioProcessor::isCurrentPatchEdited() const
{
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
    // The APVTS defaults are authored to match this column, so after
    // initialize() the engine, the APVTS and the UI all agree from the first
    // sample. There is deliberately no transient program-15 state.
    synthWrapper->synth.initialize();
    synthWrapper->synth.program_change(0);

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
    startTimer (15);
}

PRA32ColorcoderAudioProcessor::~PRA32ColorcoderAudioProcessor()
{
    stopTimer();

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
    return 0.0;
}

int PRA32ColorcoderAudioProcessor::getNumPrograms()
{
    return 16;
}

int PRA32ColorcoderAudioProcessor::getCurrentProgram()
{
    return juce::jmax (0, juce::jmin (15, currentFactoryPreset));
}

void PRA32ColorcoderAudioProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < 16)
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

void PRA32ColorcoderAudioProcessor::ensureFactoryPresetCache()
{
    if (factoryPresetCacheReady)
        return;

    const auto& params = SynthParameters::getParameters();

    factoryPresetValues.assign (params.size(),
                                std::array<int, PRA32MidiState::kFactoryProgramCount> {});

    for (size_t i = 0; i < params.size(); ++i)
        factoryPresetValues[i].fill (params[i].def);

    juce::var parsedJson = juce::JSON::parse (FactoryPresets::json());

    if (auto* obj = parsedJson.getDynamicObject())
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            for (auto& prop : obj->getProperties())
            {
                if (prop.name.toString().trim() != params[i].presetKey)
                    continue;

                if (auto* arr = prop.value.getArray())
                    if (arr->size() > 1 && arr->getReference (1).isArray())
                        if (auto* presetArr = arr->getReference (1).getArray())
                            for (int program = 0;
                                 program < PRA32MidiState::kFactoryProgramCount
                                     && program < presetArr->size();
                                 ++program)
                                factoryPresetValues[i][(size_t) program] =
                                    presetArr->getReference (program);

                break;
            }
        }
    }

    factoryPresetCacheReady = true;
}

void PRA32ColorcoderAudioProcessor::writeProgramValuesToAPVTS (int program)
{
    if (! PRA32MidiState::isValidFactoryProgram (program))
        return;

    ensureFactoryPresetCache();

    const auto& params = SynthParameters::getParameters();

    for (size_t i = 0; i < params.size() && i < factoryPresetValues.size(); ++i)
        if (auto* param = apvts.getParameter (params[i].id))
            param->setValueNotifyingHost (
                param->convertTo0to1 ((float) factoryPresetValues[i][(size_t) program]));
}

void PRA32ColorcoderAudioProcessor::captureBaselineFromProgram (int program)
{
    patchBaseline.clear();

    if (! PRA32MidiState::isValidFactoryProgram (program))
        return;

    ensureFactoryPresetCache();

    const auto& params = SynthParameters::getParameters();

    for (size_t i = 0; i < params.size(); ++i)
        patchBaseline.push_back (i < factoryPresetValues.size()
                                     ? factoryPresetValues[i][(size_t) program]
                                     : params[i].def);
}

void PRA32ColorcoderAudioProcessor::finishProgramBookkeeping (int program)
{
    // The baseline is the pure factory column, so any host/GUI value that won
    // after the program change is correctly reported as an edit.
    captureBaselineFromProgram (program);
    currentFactoryPreset = program;
    setUiProperty ("uiPreset", currentFactoryPreset);
    sendChangeMessage();
}

void PRA32ColorcoderAudioProcessor::mirrorProgramToAPVTSAndUI (int program)
{
    if (! PRA32MidiState::isValidFactoryProgram (program))
        return;

    const DeferredWriteScope scope (applyDeferredInProgress);

    writeProgramValuesToAPVTS (program);
    finishProgramBookkeeping (program);
}

void PRA32ColorcoderAudioProcessor::applyProgramToEngine (int program) noexcept
{
    // Realtime path: the engine is mutated immediately, at the MIDI event sample.
    synthWrapper->synth.program_change ((uint8_t) program);
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
    discardPendingParameterUpdates();
    clearPendingProgram();

    juce::var parsedJson = juce::JSON::parse(jsonString);
    if (!parsedJson.isObject()) return;
    
    auto* obj = parsedJson.getDynamicObject();

    const DeferredWriteScope scope (applyDeferredInProgress);

    for (const auto& p : SynthParameters::getParameters())
    {
        juce::String presetKey = p.presetKey;
        
        juce::var paramVar;
        bool found = false;
        for (auto& prop : obj->getProperties()) {
            if (prop.name.toString().trim() == presetKey) {
                paramVar = prop.value;
                found = true;
                break;
            }
        }
        
        if (found) {
            int newValue = p.def;
            if (paramVar.isArray()) {
                auto* arr = paramVar.getArray();
                if (arr->size() > 0) {
                    if (arr->getReference(0).isArray()) {
                        auto* currentArr = arr->getReference(0).getArray();
                        if (currentArr->size() > 0) {
                            newValue = currentArr->getReference(0);
                        }
                    } else {
                        newValue = arr->getReference(0);
                    }
                }
            } else {
                newValue = (int)paramVar;
            }
            
            if (auto* param = apvts.getParameter(p.id)) {
                param->setValueNotifyingHost(param->convertTo0to1((float)newValue));
            }
        }
    }

    capturePatchBaseline();
    currentFactoryPreset = -1;
    setUiProperty ("uiPreset", -1);
    sendChangeMessage();
}

juce::String PRA32ColorcoderAudioProcessor::savePresetToJson()
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    
    for (const auto& p : SynthParameters::getParameters())
    {
        juce::String presetKey = p.presetKey;
        float currentVal = *apvts.getRawParameterValue(p.id);
        obj->setProperty(presetKey, (int)currentVal);
    }
    
    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
void PRA32ColorcoderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The PRA32 engine operates at a fixed 48kHz.
    // Calculate the phase increment for our linear interpolator resampler.
    phaseIncrement = 48000.0 / sampleRate;
    currentPhase = 1.0; // Force generating the first sample on first pass
    lastL = 0.0f;
    lastR = 0.0f;
    nextL = 0.0f;
    nextR = 0.0f;
}

void PRA32ColorcoderAudioProcessor::releaseResources()
{
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

    // 1. Apply block-boundary parameter changes (GUI, host automation, state
    //    recall, preset loads) to the engine.
    updateEngineFromParameters();

    // 2. Inject the on-screen keyboard's events into the MIDI stream.
    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

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

    for (int i = 0; i < numSamples; ++i)
    {
        // Fetch new samples from the 48kHz engine as needed.
        while (currentPhase >= 1.0)
        {
            lastL = nextL;
            lastR = nextR;

            int16_t right_out = 0;
            int16_t left_out = synthWrapper->synth.process (0, 0, right_out);

            nextL = left_out / 32768.0f;
            nextR = right_out / 32768.0f;

            currentPhase -= 1.0;
        }

        const float outL = lastL + (nextL - lastL) * (float) currentPhase;
        const float outR = lastR + (nextR - lastR) * (float) currentPhase;

        channelDataL[startSample + i] = outL;

        if (channelDataR != nullptr)
            channelDataR[startSample + i] = outR;

        currentPhase += phaseIncrement;
    }
}

void PRA32ColorcoderAudioProcessor::updateEngineFromParameters()
{
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
    }
    else if (msg.isNoteOff())
    {
        synthWrapper->synth.note_off (msg.getNoteNumber());
    }
    else if (msg.isController())
    {
        const int controller = msg.getControllerNumber();
        const int value = msg.getControllerValue();

        // --- Program Change by CC (CC112..CC119) -----------------------------
        // Forwarding these to the engine lets it perform the program change
        // itself on the 0->1 gate. We must not call program_change() again, but
        // we do enqueue the deferred APVTS/UI/browser sync.
        const int pcByCcIndex = PRA32MidiState::pcByCcProgramIndex (controller);

        if (pcByCcIndex >= 0)
        {
            const int previousValue = pcByCcValues[(size_t) pcByCcIndex];
            pcByCcValues[(size_t) pcByCcIndex] = value;

            synthWrapper->synth.control_change ((uint8_t) controller, (uint8_t) value);

            if (PRA32MidiState::pcByCcTriggers (previousValue, value))
                queueProgramChange (pcByCcIndex);

            return;
        }

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
        // programs (> 15) are ignored, matching the engine's program_change().
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
}

void PRA32ColorcoderAudioProcessor::flushDeferredUpdates()
{
    // Unified deferred timeline: a Program Change is a global event, a MIDI CC
    // is a per-parameter event and a GUI/host edit is a per-parameter event.
    // They all carry a sequence from the same generator, so for each parameter
    // we simply apply the one with the highest sequence ("latest intentional
    // event wins"). This is what lets "Program Change -> CC" keep the CC while
    // "CC -> Program Change" lets the preset win.
    const auto program = pendingProgram.peek();
    const bool hasProgram = program.sequence != PRA32MidiState::kNoSequence
                            && PRA32MidiState::isValidFactoryProgram (program.program);

    if (hasProgram)
        ensureFactoryPresetCache();

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
                (hasProgram && i < (int) factoryPresetValues.size())
                    ? factoryPresetValues[(size_t) i][(size_t) program.program]
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

void PRA32ColorcoderAudioProcessor::timerCallback()
{
    flushDeferredUpdates();
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
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PRA32ColorcoderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            discardPendingParameterUpdates();
            clearPendingProgram();

            {
                const DeferredWriteScope scope (applyDeferredInProgress);
                apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            }

            capturePatchBaseline();

            const auto presetVar = getUiProperty ("uiPreset");
            currentFactoryPreset = presetVar.isVoid() ? -1 : (int) presetVar;
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

#pragma once

#include <JuceHeader.h>

// Forward declaration of the wrapper class
class PRA32Wrapper;

#include "SynthParameters.h"
#include "PRA32MidiState.h"
#include <array>
#include <atomic>
#include <vector>

struct ParamBinding {
    int cc = 0;
    std::atomic<float>* valuePtr = nullptr;
    float lastValue = -1.0f;

    // Audio-thread-only bookkeeping for MIDI-CC-driven parameter overrides.
    // While `midiPending` is true the APVTS value is lagging behind the engine
    // and must not be pushed back until it catches up (or the user changes it).
    bool midiPending = false;
    int midiTarget = -1;
    float preMidiValue = -1.0f;
    int midiPendingBlocks = 0; // safety valve so an override can never stick forever
    PRA32MidiState::Sequence midiSequence = PRA32MidiState::kNoSequence;
};
// -----------------------------------------------------------------------------

class PRA32ColorcoderAudioProcessor  : public juce::AudioProcessor,
                                       public juce::ChangeBroadcaster,
                                       private juce::AudioProcessorValueTreeState::Listener,
                                       private juce::Timer
{
public:
    PRA32ColorcoderAudioProcessor();
    ~PRA32ColorcoderAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Direct access to APVTS for the Editor
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    juce::MidiKeyboardState keyboardState;

    void loadPreset(int index);
    void loadPresetFromJson(const juce::String& jsonString);
    juce::String savePresetToJson();

    static const char* const factoryPresetNames[16];
    static juce::String factoryPresetName (int index);

    // -1 when the current patch came from a JSON file / host session.
    int getCurrentFactoryPreset() const noexcept { return currentFactoryPreset; }

    // True when any parameter differs from the patch captured at load time.
    bool isCurrentPatchEdited() const;

    // Small non-parameter UI state persisted with the plugin state.
    juce::var getUiProperty (const juce::Identifier& key) const;
    void setUiProperty (const juce::Identifier& key, const juce::var& value);

    // Applies queued audio-thread parameter/program requests to the APVTS.
    // Called by the internal timer on the message thread; exposed so the
    // automated tests can drive it without a running message loop.
    void flushDeferredUpdates();

private:
    // -------------------------------------------------------------------------
    // Core Engine Instantiation
    // -------------------------------------------------------------------------
    std::unique_ptr<PRA32Wrapper> synthWrapper;
    
    // -------------------------------------------------------------------------
    // JUCE Value Tree State for Parameter Management
    // -------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    std::vector<ParamBinding> paramBindings;

    // Precomputed CC -> parameter index map (read on the audio thread).
    std::array<int, 128> ccToParamIndex {};

    // MIDI-CC values waiting to be written back into the APVTS on the message
    // thread. The audio thread only publishes into this lock-free mailbox; the
    // message thread drains it, so no allocation or locking happens on the
    // audio path. Each entry carries a sequence and a GUI/host watermark so an
    // obsolete MIDI value can never overwrite a newer event.
    PRA32MidiState::PendingParameterMailbox parameterMailbox;

    // Last value seen on each PC-by-CC controller (CC112..119). Audio-thread
    // only; used to detect the engine's 0->1 program-change gate.
    std::array<int, 8> pcByCcValues {};

    // Single monotonic source of event sequence numbers. MIDI CC, Program Change
    // and GUI/host edits all draw from it so "latest intentional event wins" can
    // be enforced with a plain comparison during the deferred flush.
    PRA32MidiState::SequenceGenerator sequences;

    // Queued MIDI Program Change (from a real Program Change or a PC-by-CC
    // edge). The engine is already updated sample-accurately; this only drives
    // the deferred APVTS/UI/browser synchronization.
    PRA32MidiState::PendingProgramSlot pendingProgram;

    // Set while the message thread writes parameters itself (preset mirror or
    // deferred flush) so the APVTS listener does not mistake our own writes for
    // a GUI/host take-over.
    std::atomic<bool> applyDeferredInProgress { false };

    juce::RangedAudioParameter* parameterForIndex (int index) const;
    void buildCcMap();
    int indexForParameterId (const juce::String& parameterID) const;

    // APVTS listener: records a GUI/host take-over sequence for a parameter.
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // Audio thread: stamp a Program Change into the deferred mailbox.
    void queueProgramChange (int program) noexcept;
    void clearPendingProgram() noexcept;

    // Message thread: write one parameter through the APVTS and host.
    void applyParameterValue (int index, int value);

    // Message thread: apply every parameter of a factory program (user-initiated
    // preset load) and finish the bookkeeping.
    void writeProgramValuesToAPVTS (int program);
    void captureBaselineFromProgram (int program);
    void finishProgramBookkeeping (int program);

    void renderAudioRange (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void updateEngineFromParameters();
    void handleMidiMessage (const juce::MidiMessage& msg);

    // --- Preset pipeline -----------------------------------------------------
    // Realtime path: mutate the engine only. No allocation, safe on the audio
    // thread (used for sample-accurate MIDI Program Change).
    void applyProgramToEngine (int program) noexcept;

    // Message-thread path: mirror a preset onto the APVTS, the host, the preset
    // browser and the modified-state bookkeeping.
    void mirrorProgramToAPVTSAndUI (int program);

    // Lazily parses FactoryPresets::json() once into a [param][program] table so
    // a program sync no longer re-parses JSON on every change.
    void ensureFactoryPresetCache();
    std::vector<std::array<int, PRA32MidiState::kFactoryProgramCount>> factoryPresetValues;
    bool factoryPresetCacheReady = false;

    void timerCallback() override;

    // Drops any queued MIDI-CC parameter writes; used when a preset/state load
    // takes ownership of the parameters.
    void discardPendingParameterUpdates() noexcept;

    int currentFactoryPreset = 0;
    std::vector<int> patchBaseline;
    void capturePatchBaseline();

    // Resampling state for the 48kHz engine
    double currentPhase = 0.0;
    double phaseIncrement = 0.0;
    float lastL = 0.0f, lastR = 0.0f;
    float nextL = 0.0f, nextR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PRA32ColorcoderAudioProcessor)
};

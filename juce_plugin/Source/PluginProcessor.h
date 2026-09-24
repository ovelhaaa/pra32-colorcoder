#pragma once

#include <JuceHeader.h>

// Forward declaration of the wrapper class
class PRA32Wrapper;

#include "SynthParameters.h"
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
};
// -----------------------------------------------------------------------------

class PRA32ColorcoderAudioProcessor  : public juce::AudioProcessor,
                                       public juce::ChangeBroadcaster,
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
    // thread. -1 means "nothing pending". The audio thread only stores integers;
    // the message thread drains them, so no allocation or locking happens on the
    // audio path.
    static constexpr int kMaxParameters = 128;
    std::array<std::atomic<int>, kMaxParameters> pendingParameterValue {};
    std::atomic<int> pendingProgramChange { -1 };

    juce::RangedAudioParameter* parameterForIndex (int index) const;
    void buildCcMap();

    void renderAudioRange (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void updateEngineFromParameters();
    void handleMidiMessage (const juce::MidiMessage& msg);

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

#pragma once

#include <JuceHeader.h>

// Forward declaration of the wrapper class
class PRA32Wrapper;

#include "SynthParameters.h"
#include <vector>

struct ParamBinding {
    int cc;
    std::atomic<float>* valuePtr = nullptr;
    float lastValue = -1.0f;
};
// -----------------------------------------------------------------------------

class PRA32ColorcoderAudioProcessor  : public juce::AudioProcessor,
                                       public juce::ChangeBroadcaster
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

private:
    // -------------------------------------------------------------------------
    // Core Engine Instantiation
    // Based on `web_app/wasm_wrapper.cpp`
    // -------------------------------------------------------------------------
    std::unique_ptr<PRA32Wrapper> synthWrapper;
    
    // -------------------------------------------------------------------------
    // JUCE Value Tree State for Parameter Management
    // -------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    std::vector<ParamBinding> paramBindings;

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

#pragma once

#include <JuceHeader.h>

// -----------------------------------------------------------------------------
// DUMMY HEADERS / WRAPPER DEFINITIONS FOR THE EMBEDDED ENGINE
// -----------------------------------------------------------------------------
// As seen in web_app/wasm_wrapper.cpp, the engine requires these headers.
// By including web_app/ in target_include_directories, we satisfy them.
#include "Arduino.h"
#include "EEPROM.h"
#include "I2S.h"

// Definition required for the engine (avoids specific embedded hardware calls)
#ifndef PRA32_U2_USE_EMULATED_EEPROM
#define PRA32_U2_USE_EMULATED_EEPROM 1
#endif

// Variables normally defined by the Arduino sketch / wrapper
extern EEPROMClass EEPROM;
extern I2SClass g_i2s_output;
extern uint8_t g_midi_ch;

// Provide dummy digitalWrite for JUCE environment
inline void digitalWrite(uint8_t pin, uint8_t val) {}

// Include the core PRA32-U2 engine
#include "pra32-u2-synth.h"
#include "SynthParameters.h"
#include <vector>

struct ParamBinding {
    int cc;
    std::atomic<float>* valuePtr = nullptr;
    float lastValue = -1.0f;
};
// -----------------------------------------------------------------------------

class PRA32ColorcoderAudioProcessor  : public juce::AudioProcessor
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

private:
    // -------------------------------------------------------------------------
    // Core Engine Instantiation
    // Based on `web_app/wasm_wrapper.cpp`
    // -------------------------------------------------------------------------
    PRA32_U2_Synth<false, false, false, 4> synth;
    
    // -------------------------------------------------------------------------
    // JUCE Value Tree State for Parameter Management
    // -------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    std::vector<ParamBinding> paramBindings;

    // Resampling state for the 48kHz engine
    double currentPhase = 0.0;
    double phaseIncrement = 0.0;
    float lastL = 0.0f, lastR = 0.0f;
    float nextL = 0.0f, nextR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PRA32ColorcoderAudioProcessor)
};

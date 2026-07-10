#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PRA32LookAndFeel.h"
#include "SynthParameters.h"
#include <vector>
#include <memory>

class PRA32TabComponent : public juce::Component
{
public:
    PRA32TabComponent(PRA32ColorcoderAudioProcessor& p, const juce::String& tabName);
    ~PRA32TabComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PRA32ColorcoderAudioProcessor& audioProcessor;
    juce::String tabName;
    
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PRA32TabComponent)
};

class PRA32ColorcoderAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    PRA32ColorcoderAudioProcessorEditor (PRA32ColorcoderAudioProcessor&);
    ~PRA32ColorcoderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PRA32ColorcoderAudioProcessor& audioProcessor;
    PRA32LookAndFeel customLookAndFeel;

    juce::TabbedComponent tabbedComponent;
    juce::MidiKeyboardComponent keyboardComponent;

    juce::ComboBox presetComboBox;
    juce::TextButton loadButton{"Load"};
    juce::TextButton saveButton{"Save"};
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PRA32ColorcoderAudioProcessorEditor)
};

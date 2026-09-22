#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PRA32LookAndFeel.h"
#include "PRA32Components.h"
#include "SynthParameters.h"
#include <vector>
#include <map>
#include <memory>

// A page renders one section (OSC / FILTER / ...) with a section-specific
// composition built from the shared widgets and UI metadata.
class PRA32ParameterPage : public juce::Component
{
public:
    PRA32ParameterPage (PRA32ColorcoderAudioProcessor& processor, juce::String sectionName);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ModulePanel* addModule (const juce::String& title);
    juce::Component* addParam (const juce::String& id);
    void addToPanel (ModulePanel& panel, const juce::String& id);
    void addToPanel (ModulePanel& panel, std::initializer_list<const char*> ids);

    juce::Component* find (const juce::String& id) const;

    void buildGeneric();
    void buildOsc();
    void buildFilter();
    void buildEnvs();
    void buildMod();
    void buildFx();
    void buildColor();

    void layoutStack (juce::Rectangle<int> area, int gap);
    void layoutOsc (juce::Rectangle<int> area);
    void layoutFilter (juce::Rectangle<int> area);
    void layoutEnvs (juce::Rectangle<int> area);
    void layoutMod (juce::Rectangle<int> area);
    void layoutFx (juce::Rectangle<int> area);
    void layoutColor (juce::Rectangle<int> area);

    PRA32ColorcoderAudioProcessor& processor;
    juce::String section;

    juce::OwnedArray<ModulePanel> panels;
    juce::OwnedArray<juce::Component> controls;
    juce::OwnedArray<EnvelopePanel> envelopes;
    juce::OwnedArray<FilterResponseGraph> graphs;

    std::map<juce::String, juce::Component*> controlById;
    std::map<juce::String, ModulePanel*> moduleById;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PRA32ParameterPage)
};

//==============================================================================
class PRA32ColorcoderAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit PRA32ColorcoderAudioProcessorEditor (PRA32ColorcoderAudioProcessor&);
    ~PRA32ColorcoderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showPage (int index);
    void loadPreset (int index);
    void updatePresetDisplay();
    void showPresetMenu();

    static const char* presetName (int index);

    PRA32ColorcoderAudioProcessor& audioProcessor;
    PRA32LookAndFeel customLookAndFeel;

    juce::OwnedArray<PRA32ParameterPage> pages;
    SectionSelector sectionSelector;

    juce::TextButton presetPrevButton { "<" };
    juce::TextButton presetNextButton { ">" };
    ValueReadout presetDisplay;
    juce::TextButton loadButton { "LOAD JSON" };
    juce::TextButton saveButton { "SAVE JSON" };
    juce::TextButton keyboardButton { "KEYS" };

    PRA32Keyboard keyboardComponent;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Rectangle<int> keyboardFrame;
    juce::Rectangle<int> keyboardKeys;

    int presetIndex = 0;
    bool keyboardVisible = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PRA32ColorcoderAudioProcessorEditor)
};

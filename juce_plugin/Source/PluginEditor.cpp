#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PRA32TabComponent::PRA32TabComponent(PRA32ColorcoderAudioProcessor& p, const juce::String& tab)
    : audioProcessor(p), tabName(tab)
{
    auto params = SynthParameters::getParameters();
    for (const auto& param : params)
    {
        if (param.tab == tabName)
        {
            auto slider = std::make_unique<juce::Slider>();
            if (param.type == "h") {
                slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 16);
            } else {
                slider->setSliderStyle(juce::Slider::LinearHorizontal);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
            }
            
            addAndMakeVisible(slider.get());
            
            auto label = std::make_unique<juce::Label>("", param.label);
            label->setJustificationType(juce::Justification::centred);
            label->attachToComponent(slider.get(), false);
            label->setFont(juce::Font(10.0f));
            label->setColour(juce::Label::textColourId, juce::Colour(0xff555555));
            addAndMakeVisible(label.get());
            
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.getAPVTS(), param.id, *slider);
                
            sliders.push_back(std::move(slider));
            labels.push_back(std::move(label));
            attachments.push_back(std::move(attachment));
        }
    }
}

PRA32TabComponent::~PRA32TabComponent()
{
}

void PRA32TabComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141414)); // Panel background
}

void PRA32TabComponent::resized()
{
    // A simple grid layout
    int numColumns = 4;
    int itemWidth = getWidth() / numColumns;
    int itemHeight = 80;
    
    int row = 0;
    int col = 0;
    
    for (auto& slider : sliders)
    {
        int x = col * itemWidth;
        int y = row * itemHeight + 20; // +20 for label space
        
        slider->setBounds(x + 10, y, itemWidth - 20, itemHeight - 20);
        
        col++;
        if (col >= numColumns) {
            col = 0;
            row++;
        }
    }
}

//==============================================================================
PRA32ColorcoderAudioProcessorEditor::PRA32ColorcoderAudioProcessorEditor (PRA32ColorcoderAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), 
      tabbedComponent(juce::TabbedButtonBar::Orientation::TabsAtTop),
      keyboardComponent(audioProcessor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&customLookAndFeel);

    // Add Tabs
    std::vector<juce::String> tabs = {"OSC", "FILTER", "ENVS", "MOD", "FX", "LO-FI"};
    for (const auto& tab : tabs)
    {
        tabbedComponent.addTab(tab, juce::Colour(0xff1E1E1E), new PRA32TabComponent(audioProcessor, tab), true);
    }
    
    const char* presetNames[] = {
        "00 · Initialization", "01 · Sync Lead", "02 · Synth Brass", "03 · Pluck Synth",
        "04 · Mono Synth", "05 · Synth Bass 1", "06 · Synth Bass 2", "07 · Synth Bass 3",
        "08 · Ethereal Pad", "09 · Gritty Bass", "10 · Chiptune Lead", "11 · Percussive Pluck",
        "12 · Classic Sweep", "13 · Dark Drone", "14 · Noise Percussion", "15 · Bell Lead"
    };
    for (int i = 0; i < 16; ++i) {
        presetComboBox.addItem(presetNames[i], i + 1);
    }
    presetComboBox.setTextWhenNothingSelected("Select Preset...");
    
    presetComboBox.onChange = [this]() {
        audioProcessor.loadPreset(presetComboBox.getSelectedId() - 1);
    };

    loadButton.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>("Load Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json");
        auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(folderChooserFlags, [this] (const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                audioProcessor.loadPresetFromJson(file.loadFileAsString());
            }
        });
    };

    saveButton.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>("Save Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json");
        auto folderChooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(folderChooserFlags, [this] (const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File{}) {
                file.replaceWithText(audioProcessor.savePresetToJson());
            }
        });
    };

    addAndMakeVisible(presetComboBox);
    addAndMakeVisible(loadButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(tabbedComponent);
    addAndMakeVisible(keyboardComponent);
    
    keyboardComponent.setAvailableRange(36, 96);
    keyboardComponent.setKeyWidth(32);
    keyboardComponent.setBlackNoteWidthProportion(0.6f);

    setSize (600, 500);
}

PRA32ColorcoderAudioProcessorEditor::~PRA32ColorcoderAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void PRA32ColorcoderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour(0xff0E0E0E)); // Root background
    
    // Header
    g.setColour(juce::Colour(0xff141414));
    g.fillRect(0, 0, getWidth(), 40);
    g.setColour(juce::Colour(0xff2A2A2A));
    g.drawLine(0, 40.0f, (float)getWidth(), 40.0f);
    
    g.setColour(juce::Colour(0xffE8A020));
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("PRA32 COLORCODER", 20, 0, 300, 40, juce::Justification::centredLeft);
}

void PRA32ColorcoderAudioProcessorEditor::resized()
{
    tabbedComponent.setBounds(20, 60, getWidth() - 40, getHeight() - 170);
    keyboardComponent.setBounds(20, getHeight() - 90, getWidth() - 40, 70);
    
    int rightMargin = getWidth() - 20;
    saveButton.setBounds(rightMargin - 60, 10, 60, 20);
    loadButton.setBounds(saveButton.getX() - 70, 10, 60, 20);
    presetComboBox.setBounds(loadButton.getX() - 160, 10, 150, 20);
}

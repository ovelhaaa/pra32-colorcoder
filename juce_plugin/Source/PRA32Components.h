#pragma once

#include <JuceHeader.h>
#include <array>
#include "SynthParameters.h"
#include "PRA32Theme.h"

// -----------------------------------------------------------------------------
// Reusable panel widgets. All of them are attachment-compatible with the APVTS
// and none of them know anything about the DSP.
// -----------------------------------------------------------------------------

class ParameterKnob : public juce::Slider
{
public:
    ParameterKnob (const SynthParamData& info, juce::AudioProcessorValueTreeState& apvts);

    const SynthParamData& parameter() const noexcept { return param; }

    juce::String getTooltip() override;
    void paint (juce::Graphics&) override;

private:
    SynthParamData param;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterKnob)
};

// -----------------------------------------------------------------------------
class SteppedSelector : public juce::Slider
{
public:
    SteppedSelector (const SynthParamData& info, juce::AudioProcessorValueTreeState& apvts);

    const SynthParamData& parameter() const noexcept { return param; }

    // 0 = single row; otherwise the push-button bank wraps into a grid of
    // this many columns (e.g. 3 columns x 2 rows for six positions).
    void setGridColumns (int columns);

    juce::String getTooltip() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void selectFromPoint (juce::Point<int> position);
    int  gridColumnsFor (int itemCount) const;

    SynthParamData param;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    int gridColumns = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteppedSelector)
};

// -----------------------------------------------------------------------------
class ToggleSwitch : public juce::Button
{
public:
    ToggleSwitch (const SynthParamData& info, juce::AudioProcessorValueTreeState& apvts);

    juce::String getTooltip() override;
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

private:
    SynthParamData param;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToggleSwitch)
};

// -----------------------------------------------------------------------------
// A framed, titled module that lays out its children in a responsive grid.
class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::String title, juce::String subsection, juce::Colour accent);

    void addControl (juce::Component& control, int visualWeight = 2);
    void paint (juce::Graphics&) override;
    void resized() override;

    int preferredColumns() const noexcept { return columnsHint; }
    void setPreferredColumns (int c) noexcept { columnsHint = juce::jmax (1, c); }
    void setAutoLayout (bool shouldAutoLayout) noexcept { autoLayout = shouldAutoLayout; }

    // Highlights the heaviest control as a large "hero" on the left, with the
    // remaining controls in a grid to its right.
    void setHeroLayout (bool shouldUseHero) noexcept { heroLayout = shouldUseHero; }

private:
    juce::String title;
    juce::String subsection;
    juce::Colour accent;
    int columnsHint = 4;
    bool autoLayout = true;
    bool heroLayout = false;
    juce::Array<juce::Component*> controls;
    juce::Array<int> weights;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulePanel)
};

// -----------------------------------------------------------------------------
// Physical section navigation: segmented switches with an annunciator LED.
class SectionSelector : public juce::Component
{
public:
    SectionSelector();

    void setSections (const juce::StringArray& names);
    void setActive (int index, juce::NotificationType notification = juce::sendNotificationSync);
    int  getActive() const noexcept { return active; }

    std::function<void (int)> onSectionChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::StringArray sections;
    int active = 0;
    int hover = -1;

    juce::Rectangle<int> boundsFor (int index) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionSelector)
};

// -----------------------------------------------------------------------------
// ADSR envelope module: a live UI-only curve plus the four stage knobs.
class EnvelopePanel : public juce::Component,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    EnvelopePanel (juce::AudioProcessorValueTreeState& apvts,
                   const SynthParamData& attack, const SynthParamData& decay,
                   const SynthParamData& sustain, const SynthParamData& release,
                   juce::Colour accent);
    ~EnvelopePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String& id, float newValue) override;

    juce::AudioProcessorValueTreeState& state;
    std::array<SynthParamData, 4> params;
    std::array<float, 4> props { 0.0f, 0.5f, 1.0f, 0.5f };
    juce::OwnedArray<ParameterKnob> knobs;
    juce::Colour accent;
    juce::Rectangle<int> curveArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopePanel)
};

// -----------------------------------------------------------------------------
// UI-only filter magnitude sketch. Never touches the audio path.
class FilterResponseGraph : public juce::Component,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    FilterResponseGraph (juce::AudioProcessorValueTreeState& apvts, juce::Colour accent);
    ~FilterResponseGraph() override;

    void paint (juce::Graphics&) override;

private:
    void parameterChanged (const juce::String& id, float newValue) override;

    juce::AudioProcessorValueTreeState& state;
    float cutoffValue = 127.0f;
    float resoValue = 0.0f;
    float modeValue = 0.0f;
    juce::Colour accent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterResponseGraph)
};

// -----------------------------------------------------------------------------
// Themed MIDI keyboard: ivory white keys and glossy black keys with the same
// top-left lighting as the rest of the panel.
class PRA32Keyboard : public juce::MidiKeyboardComponent
{
public:
    explicit PRA32Keyboard (juce::MidiKeyboardState& state);

    void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour,
                        juce::Colour textColour) override;

    void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PRA32Keyboard)
};

// -----------------------------------------------------------------------------
// A non-editable technical readout (preset number / name, status text).
class ValueReadout : public juce::Component
{
public:
    ValueReadout();

    void setText (const juce::String& primary, const juce::String& secondary = {});
    void setJustification (juce::Justification j) { justification = j; repaint(); }
    void setPrimaryFont (juce::Font f) { primaryFont = std::move (f); repaint(); }
    void setColours (juce::Colour primary, juce::Colour secondary);

    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String primary, secondary;
    juce::Justification justification { juce::Justification::centredLeft };
    juce::Font primaryFont { PRA32Theme::presetFont() };
    juce::Colour primaryColour { PRA32Theme::amber };
    juce::Colour secondaryColour { PRA32Theme::textDim };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueReadout)
};

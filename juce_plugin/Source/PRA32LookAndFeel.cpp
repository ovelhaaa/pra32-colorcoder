#include "PRA32LookAndFeel.h"

using namespace PRA32Theme;

PRA32LookAndFeel::PRA32LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, chassis);
    setColour (juce::Slider::textBoxTextColourId,        textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId,  panelSunken);
    setColour (juce::Slider::textBoxOutlineColourId,     border);
    setColour (juce::Label::textColourId,                textPrimary);
    setColour (juce::TextButton::buttonColourId,         panelRaised);
    setColour (juce::TextButton::buttonOnColourId,       amber);
    setColour (juce::TextButton::textColourOffId,        textPrimary);
    setColour (juce::TextButton::textColourOnId,         chassis);
    setColour (juce::ComboBox::backgroundColourId,       panelSunken);
    setColour (juce::ComboBox::textColourId,             textPrimary);
    setColour (juce::ComboBox::outlineColourId,          border);
    setColour (juce::ComboBox::arrowColourId,            textSecondary);
    setColour (juce::PopupMenu::backgroundColourId,      panelRaised);
    setColour (juce::PopupMenu::textColourId,            textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, textPrimary);
    setColour (juce::TextEditor::backgroundColourId,     panelSunken);
    setColour (juce::TextEditor::textColourId,           textPrimary);
    setColour (juce::TextEditor::outlineColourId,        border);
    setColour (juce::TextEditor::focusedOutlineColourId, amber);
    setColour (juce::TextEditor::highlightColourId,      amber.withAlpha (0.35f));
    setColour (juce::ScrollBar::thumbColourId,           separator);
}

//==============================================================================
void PRA32LookAndFeel::drawPanelSurface (juce::Graphics& g, juce::Rectangle<float> area,
                                         float radius, bool raised)
{
    if (raised)
    {
        PRA32Theme::drawRaisedPlate (g, area, radius);
        return;
    }

    g.setColour (panel);
    g.fillRoundedRectangle (area, radius);

    g.setColour (border);
    g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);

    // Lighting comes from the upper-left.
    g.setColour (bevelLight);
    g.drawLine (area.getX() + radius, area.getY() + 1.0f,
                area.getRight() - radius, area.getY() + 1.0f, 1.0f);
    g.setColour (bevelDark);
    g.drawLine (area.getX() + radius, area.getBottom() - 1.0f,
                area.getRight() - radius, area.getBottom() - 1.0f, 1.0f);
}

void PRA32LookAndFeel::drawFastener (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    PRA32Theme::drawScrew (g, centre, radius);
}

void PRA32LookAndFeel::drawLed (juce::Graphics& g, juce::Rectangle<float> area, bool on,
                                juce::Colour onColour)
{
    PRA32Theme::drawLamp (g, area, onColour, on ? 1.0f : 0.0f, 2.0f);
}

//==============================================================================
void PRA32LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, const float rotaryStartAngle,
                                         const float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds   = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    auto centre    = bounds.getCentre();
    auto diameter  = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto outerR    = diameter * 0.5f - 1.0f;
    auto bodyR     = outerR * 0.66f;
    auto angle     = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto accent    = slider.findColour (juce::Slider::rotarySliderFillColourId);

    // Scale / serigraphy ticks belong to the panel, not to the knob body.
    const int tickCount = 11;
    for (int i = 0; i < tickCount; ++i)
    {
        const float t = (float) i / (float) (tickCount - 1);
        const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const bool strong = (tickCount % 2 == 0) ? false : (i % 5 == 0);
        const float inner = bodyR + outerR * 0.14f;
        const float outer = outerR * 0.98f;

        g.setColour (strong ? tickStrong : tick);
        g.drawLine (centre.x + inner * std::sin (a), centre.y - inner * std::cos (a),
                    centre.x + outer * std::sin (a), centre.y - outer * std::cos (a),
                    strong ? 1.4f : 1.0f);
    }

    // Active arc, deliberately subtle.
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, bodyR + outerR * 0.16f, bodyR + outerR * 0.16f,
                       0.0f, rotaryStartAngle, angle, true);
    g.setColour (accent.withAlpha (0.55f));
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Body with an implicit upper-left highlight.
    juce::ColourGradient body (knobBodyTop, centre.x - bodyR * 0.6f, centre.y - bodyR * 0.8f,
                               knobBody, centre.x + bodyR * 0.9f, centre.y + bodyR, true);
    g.setColour (knobBody);
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre));
    g.setGradientFill (body);
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre));

    // Chamfer.
    g.setColour (bevelLight);
    g.drawEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre), 1.0f);
    g.setColour (bevelDark);
    g.drawEllipse (juce::Rectangle<float> (bodyR * 1.86f, bodyR * 1.86f).withCentre (centre), 1.0f);

    // Pointer.
    juce::Path pointer;
    const float pw = juce::jmax (2.0f, bodyR * 0.14f);
    pointer.addRoundedRectangle (-pw * 0.5f, -bodyR * 0.92f, pw, bodyR * 0.62f, pw * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (knobPointer);
    g.fillPath (pointer);
}

//==============================================================================
void PRA32LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool vertical = (style == juce::Slider::LinearVertical
                            || style == juce::Slider::LinearBarVertical);

    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    if (vertical)
    {
        const float cx = (float) x + (float) width * 0.5f;
        g.setColour (panelSunken);
        g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) height, 2.0f);
        g.setColour (separator);
        g.drawRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) height, 2.0f, 1.0f);

        const float top = (float) y + 4.0f;
        const float bot = (float) y + (float) height - 4.0f;
        g.setColour (accent.withAlpha (0.5f));
        g.drawLine (cx, sliderPos, cx, bot, 2.0f);

        g.setColour (accent);
        g.fillRoundedRectangle (cx - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f, 2.0f);
        g.setColour (knobBody);
        g.drawRoundedRectangle (cx - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f, 2.0f, 1.0f);
    }
    else
    {
        const float cy = (float) y + (float) height * 0.5f;
        g.setColour (panelSunken);
        g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) width, 4.0f, 2.0f);
        g.setColour (separator);
        g.drawRoundedRectangle ((float) x, cy - 2.0f, (float) width, 4.0f, 2.0f, 1.0f);

        g.setColour (accent.withAlpha (0.5f));
        g.drawLine ((float) x + 2.0f, cy, sliderPos, cy, 2.0f);

        g.setColour (accent);
        g.fillRoundedRectangle (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f, 2.0f);
        g.setColour (knobBody);
        g.drawRoundedRectangle (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f, 2.0f, 1.0f);
    }
}

//==============================================================================
void PRA32LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    const float radius = radiusSmall;

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (area, radius);

    g.setColour (box.isMouseOver() ? amber.withAlpha (0.6f)
                                   : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);

    // Small engraved triangle.
    juce::Path arrow;
    const float cx = (float) width - 12.0f;
    const float cy = (float) height * 0.5f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void PRA32LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font PRA32LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return valueFont();
}

//==============================================================================
void PRA32LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour&, bool highlighted, bool down)
{
    auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = radiusSmall;
    const bool on = button.getToggleState();

    juce::Colour base = on ? button.findColour (juce::TextButton::buttonOnColourId)
                           : button.findColour (juce::TextButton::buttonColourId);

    if (down)              base = base.brighter (0.18f);
    else if (highlighted)  base = base.brighter (0.08f);

    g.setColour (base);
    g.fillRoundedRectangle (area, radius);

    g.setColour (bevelLight);
    g.drawLine (area.getX() + radius, area.getY() + 1.0f,
                area.getRight() - radius, area.getY() + 1.0f, 1.0f);
    g.setColour (on ? amber : border);
    g.drawRoundedRectangle (area, radius, 1.0f);
}

void PRA32LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                       bool, bool)
{
    g.setFont (legendFont());
    g.setColour (button.findColour (button.getToggleState()
                                        ? juce::TextButton::textColourOnId
                                        : juce::TextButton::textColourOffId));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds(),
                      juce::Justification::centred, 1);
}

//==============================================================================
void PRA32LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (border);
    g.drawRect (0, 0, width, height, 1);
}

void PRA32LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool hasSubMenu, const juce::String& text,
                                          const juce::String&, const juce::Drawable*,
                                          const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (separator);
        g.fillRect (area.reduced (6, 0).removeFromTop (1));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRect (area);
    }

    g.setFont (valueFont());
    g.setColour (textColour != nullptr ? *textColour
                                       : findColour (juce::PopupMenu::textColourId)
                                             .withAlpha (isActive ? 1.0f : 0.4f));

    auto r = area.reduced (8, 0);
    if (isTicked)
        r.removeFromLeft (14);

    g.drawFittedText (text, r, juce::Justification::centredLeft, 1);

    if (isTicked)
    {
        g.setColour (amber);
        g.fillEllipse ((float) area.getX() + 4.0f, (float) area.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
}

//==============================================================================
void PRA32LookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (label.getFont());
        g.drawFittedText (label.getText(), label.getLocalBounds(),
                          label.getJustificationType(), 1);
    }
}

void PRA32LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                 juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height),
                            radiusSmall);
}

void PRA32LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                              juce::TextEditor& editor)
{
    g.setColour (editor.hasKeyboardFocus (true)
                     ? editor.findColour (juce::TextEditor::focusedOutlineColourId)
                     : editor.findColour (juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f,
                                                    (float) height - 1.0f),
                            radiusSmall, 1.0f);
}

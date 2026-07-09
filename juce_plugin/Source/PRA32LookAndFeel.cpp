#include "PRA32LookAndFeel.h"

PRA32LookAndFeel::PRA32LookAndFeel()
{
    // Web app CSS defines background as #0E0E0E and #141414 for panels
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0E0E0E));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffE8A020));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff252525));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffE8A020));
    
    // Tabbed component styling
    setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff141414));
    setColour(juce::TabbedComponent::outlineColourId, juce::Colour(0xff1E1E1E));
    setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colour(0xff2A2A2A));
    setColour(juce::TabbedButtonBar::frontTextColourId, juce::Colour(0xffE8A020));
    setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xff888888));
}

void PRA32LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, 
                                        float sliderPos, const float rotaryStartAngle, 
                                        const float rotaryEndAngle, juce::Slider& slider)
{
    auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
    auto centreX = (float) x + (float) width  * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Draw the dark background circle (like the SVG circle in the web app)
    g.setColour (juce::Colour(0xff141414));
    g.fillEllipse (rx, ry, rw, rw);
    
    g.setColour (juce::Colour(0xff333333));
    g.drawEllipse (rx, ry, rw, rw, 1.5f);

    // Draw the gold arc
    juce::Path p;
    auto pointerLength = radius * 0.8f;
    auto pointerThickness = 3.0f;
    p.addRectangle (-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    
    // Draw an arc for the track and the knob body.
    juce::Path arc;
    arc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(juce::Colour(0xffE8A020));
    g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // Draw indicator
    g.fillPath (p);
}

void PRA32LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // Replicate the web app's horizontal slider: #252525 track, #E8A020 thumb
    auto trackRect = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - 1.0f, (float) width, 2.0f);
    
    g.setColour (juce::Colour(0xff252525));
    g.fillRoundedRectangle (trackRect, 1.0f);
    
    g.setColour (juce::Colour(0xffE8A020));
    auto thumbWidth = 12.0f;
    g.fillEllipse (sliderPos - thumbWidth / 2.0f, (float) y + (float) height * 0.5f - thumbWidth / 2.0f, thumbWidth, thumbWidth);
}

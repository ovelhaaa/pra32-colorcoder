#pragma once

#include <JuceHeader.h>

class PRA32LookAndFeel : public juce::LookAndFeel_V4
{
public:
    PRA32LookAndFeel();
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, 
                          float sliderPos, const float rotaryStartAngle, 
                          const float rotaryEndAngle, juce::Slider& slider) override;
                          
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;
};

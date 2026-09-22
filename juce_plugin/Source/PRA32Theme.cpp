#include "PRA32Theme.h"

#include <cmath>

namespace PRA32Theme
{

juce::Colour sectionAccent (const juce::String& section)
{
    if (section == "OSC")       return juce::Colour (0xffd99a3f); // amber
    if (section == "FILTER")    return juce::Colour (0xff5eb7b0); // cyan / teal
    if (section == "ENVS")      return juce::Colour (0xffa8b56a); // olive
    if (section == "MOD")       return juce::Colour (0xff7f9fd0); // steel blue
    if (section == "FX")        return juce::Colour (0xffc98b6a); // copper
    if (section == "CHARACTER"
     || section == "COLOR")     return juce::Colour (0xffb07fb0); // mauve
    return amber;
}

juce::Font brandFont()
{
    return juce::Font (juce::FontOptions (20.0f).withStyle ("Bold"));
}

juce::Font subBrandFont()
{
    return juce::Font (juce::FontOptions (8.5f).withStyle ("Bold"));
}

juce::Font moduleFont()
{
    return juce::Font (juce::FontOptions (13.0f).withStyle ("Bold"));
}

juce::Font sectionFont()
{
    return juce::Font (juce::FontOptions (12.0f).withStyle ("Bold"));
}

juce::Font labelFont()
{
    return juce::Font (juce::FontOptions (10.5f));
}

juce::Font legendFont()
{
    return juce::Font (juce::FontOptions (9.0f));
}

juce::Font valueFont()
{
    return juce::Font (juce::FontOptions (11.0f)
                           .withName (juce::Font::getDefaultMonospacedFontName()));
}

juce::Font presetFont()
{
    return juce::Font (juce::FontOptions (14.0f)
                           .withName (juce::Font::getDefaultMonospacedFontName()));
}

//==============================================================================
namespace
{
    float& scaleRef()
    {
        static float s = 1.0f;
        return s;
    }
}

void setUiScale (float scale)
{
    scaleRef() = juce::jlimit (0.75f, 2.0f, scale);
}

float uiScale()
{
    return scaleRef();
}

//==============================================================================
void drawBrushedPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                       juce::Colour base, bool vertical)
{
    juce::ColourGradient grad (base.brighter (0.06f),
                               vertical ? bounds.getCentreX() : bounds.getX(),
                               vertical ? bounds.getY() : bounds.getCentreY(),
                               base.darker (0.16f),
                               vertical ? bounds.getCentreX() : bounds.getRight(),
                               vertical ? bounds.getBottom() : bounds.getCentreY(),
                               false);
    g.setGradientFill (grad);
    g.fillRect (bounds);

    const int n = vertical ? (int) bounds.getWidth() : (int) bounds.getHeight();

    for (int i = 0; i < n; ++i)
    {
        const float k = (float) i;
        const float noise = std::sin (k * 12.9898f + 4.1414f) * 43758.5453f;
        const float f = (noise - std::floor (noise)) - 0.5f;

        g.setColour (f > 0.0f ? juce::Colours::white.withAlpha (0.014f)
                              : juce::Colours::black.withAlpha (0.02f));

        if (vertical)
            g.drawVerticalLine ((int) bounds.getX() + i, bounds.getY(), bounds.getBottom());
        else
            g.drawHorizontalLine ((int) bounds.getY() + i, bounds.getX(), bounds.getRight());
    }
}

void drawRaisedPlate (juce::Graphics& g, juce::Rectangle<float> bounds, float corner)
{
    juce::ColourGradient grad (panelRaised.brighter (0.10f), bounds.getX(), bounds.getY(),
                               panelRaised.darker (0.30f), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawLine (bounds.getX() + corner, bounds.getY() + 0.8f,
                bounds.getRight() - corner, bounds.getY() + 0.8f, 1.0f);

    g.setColour (engraveShadow.withAlpha (0.55f));
    g.drawLine (bounds.getX() + corner, bounds.getBottom() - 0.8f,
                bounds.getRight() - corner, bounds.getBottom() - 0.8f, 1.6f);

    g.setColour (border.brighter (0.05f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
}

void drawInsetWell (juce::Graphics& g, juce::Rectangle<float> bounds, float corner, float depth)
{
    juce::ColourGradient grad (juce::Colour (0xff050607), bounds.getX(), bounds.getY(),
                               juce::Colour (0xff14171a), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (juce::Colours::black.withAlpha (0.5f * juce::jlimit (0.0f, 1.0f, depth)));
    g.drawLine (bounds.getX() + corner, bounds.getY() + 1.0f,
                bounds.getRight() - corner, bounds.getY() + 1.0f, 1.6f);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
}

void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (centre.x - radius - 1.0f, centre.y - radius - 1.0f,
                   radius * 2.0f + 2.0f, radius * 2.0f + 2.0f);

    juce::ColourGradient grad (metalScrew.brighter (0.28f),
                               centre.x - radius * 0.4f, centre.y - radius * 0.7f,
                               metalScrew.darker (0.34f),
                               centre.x + radius * 0.4f, centre.y + radius * 0.7f, true);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (metalHighlight.withAlpha (0.5f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 0.8f);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (centre.x - radius * 0.6f, centre.y - radius * 0.6f,
                centre.x + radius * 0.6f, centre.y + radius * 0.6f,
                juce::jmax (1.0f, radius * 0.3f));
}

void drawScrewsInCorners (juce::Graphics& g, juce::Rectangle<float> bounds, float inset, float radius)
{
    drawScrew (g, { bounds.getX() + inset, bounds.getY() + inset }, radius);
    drawScrew (g, { bounds.getRight() - inset, bounds.getY() + inset }, radius);
    drawScrew (g, { bounds.getX() + inset, bounds.getBottom() - inset }, radius);
    drawScrew (g, { bounds.getRight() - inset, bounds.getBottom() - inset }, radius);
}

void drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                       const juce::Font& font, juce::Justification justification,
                       juce::Colour faceColour, juce::Colour highlightColour)
{
    g.setFont (font);
    g.setColour (highlightColour);
    g.drawText (text, area.translated (0.0f, 1.0f), justification, false);
    g.setColour (faceColour);
    g.drawText (text, area, justification, false);
}

void drawLamp (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
               float intensity, float corner)
{
    const float amount = juce::jlimit (0.0f, 1.0f, intensity);
    drawInsetWell (g, area, corner, 0.6f);

    auto glass = area.reduced (juce::jmax (1.0f, area.getWidth() * 0.10f));

    if (amount > 0.02f)
    {
        const auto lit = colour.withMultipliedBrightness (0.7f + 0.5f * amount);

        g.setColour (lit.withAlpha (0.25f * amount));
        g.fillRoundedRectangle (glass.expanded (2.0f), corner + 1.5f);

        g.setColour (lit.withAlpha (0.35f + 0.65f * amount));
        g.fillRoundedRectangle (glass, juce::jmax (1.0f, corner - 0.5f));

        auto reflection = glass.removeFromTop (glass.getHeight() * 0.42f);
        g.setColour (juce::Colours::white.withAlpha (0.22f * amount));
        g.fillRoundedRectangle (reflection.reduced (1.0f), juce::jmax (1.0f, corner - 1.0f));
    }
    else
    {
        g.setColour (lampOff);
        g.fillRoundedRectangle (glass, juce::jmax (1.0f, corner - 0.5f));

        auto reflection = glass.removeFromTop (glass.getHeight() * 0.42f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (reflection.reduced (1.0f), juce::jmax (1.0f, corner - 1.0f));
    }
}

} // namespace PRA32Theme

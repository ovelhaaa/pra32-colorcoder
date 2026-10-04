#include "PRA32Theme.h"

#include <cmath>
#include <TC32FontData.h>

namespace PRA32Theme
{

juce::Colour sectionAccent (const juce::String& section)
{
    if (section == "OSC")       return juce::Colour (0xffd99a3f); // amber
    if (section == "FILTER")    return juce::Colour (0xff5eb7b0); // cyan / teal
    if (section == "ENVS")      return juce::Colour (0xffd7af66); // envelope amber
    if (section == "MOD")       return juce::Colour (0xff7f9fd0); // steel blue
    if (section == "FX")        return juce::Colour (0xffa399c7); // violet
    if (section == "CHARACTER"
     || section == "COLOR")     return juce::Colour (0xffc58a80); // muted red
    return amber;
}

juce::Font instrumentFont (float height, bool bold, float tracking)
{
    static const auto regular = juce::Typeface::createSystemTypefaceFor (
        TC32FontData::MontserratRegular_ttf, TC32FontData::MontserratRegular_ttfSize);
    static const auto strong = juce::Typeface::createSystemTypefaceFor (
        TC32FontData::MontserratBold_ttf, TC32FontData::MontserratBold_ttfSize);
    return juce::Font (juce::FontOptions (bold ? strong : regular))
        .withHeight (height).withExtraKerningFactor (tracking);
}

juce::Font brandFont()    { return instrumentFont (22.0f, true, 0.06f); }
juce::Font subBrandFont() { return instrumentFont (8.5f, false, 0.08f); }
juce::Font moduleFont()   { return instrumentFont (12.0f, true, 0.06f); }
juce::Font sectionFont()  { return instrumentFont (12.0f, true, 0.04f); }
juce::Font labelFont()    { return instrumentFont (11.5f, false, 0.015f); }
juce::Font legendFont()   { return instrumentFont (9.0f, false, 0.025f); }
juce::Font valueFont()    { return instrumentFont (12.0f, true, 0.0f); }
juce::Font presetFont()   { return instrumentFont (13.0f, true, 0.0f); }

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
void drawAnalyzerGrid (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour (separator.withAlpha (0.45f));
    for (int i = 0; i <= 8; ++i)
    {
        const float x = bounds.getX() + bounds.getWidth() * (float) i / 8.0f;
        g.drawLine (x, bounds.getY(), x, bounds.getBottom(), 0.6f);
    }
    for (int i = 0; i <= 4; ++i)
    {
        const float y = bounds.getY() + bounds.getHeight() * (float) i / 4.0f;
        g.drawLine (bounds.getX(), y, bounds.getRight(), y, 0.6f);
    }
}

void drawBrushedPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                       juce::Colour base, bool vertical)
{
    juce::ignoreUnused (vertical);
    g.setColour (base);
    g.fillRect (bounds);
}

void drawRaisedPlate (juce::Graphics& g, juce::Rectangle<float> bounds, float corner)
{
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
    g.setColour (bevelLight);
    g.drawLine (bounds.getX() + corner, bounds.getY() + 1.0f,
                bounds.getRight() - corner, bounds.getY() + 1.0f, 1.0f);
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
        g.fillRoundedRectangle (glass, corner);

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

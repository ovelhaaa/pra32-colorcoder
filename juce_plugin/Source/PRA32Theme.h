#pragma once

#include <JuceHeader.h>

// Centralised visual tokens for the PRA32 Colorcoder panel. Every colour,
// metric and font used by the UI lives here so paint() code stays free of
// magic numbers. Nothing here affects the DSP or parameter values.
namespace PRA32Theme
{
    // -------------------------------------------------------------------------
    // Surfaces
    // -------------------------------------------------------------------------
    inline const juce::Colour chassis      { 0xff101214 };
    inline const juce::Colour chassisEdge  { 0xff0a0b0c };
    inline const juce::Colour panel        { 0xff181a1d };
    inline const juce::Colour panelRaised  { 0xff202327 };
    inline const juce::Colour panelSunken  { 0xff131518 };
    inline const juce::Colour separator    { 0xff34383c };
    inline const juce::Colour border       { 0xff2b2f33 };
    inline const juce::Colour bevelLight   { 0x14ffffff };
    inline const juce::Colour bevelDark    { 0x40000000 };

    // -------------------------------------------------------------------------
    // Text / state
    // -------------------------------------------------------------------------
    inline const juce::Colour textPrimary  { 0xffe7e0d1 };
    inline const juce::Colour textSecondary{ 0xff9a948a };
    inline const juce::Colour textDim      { 0xff6a655d };
    inline const juce::Colour amber        { 0xffd99a3f };
    inline const juce::Colour amberBright  { 0xffe8a020 };
    inline const juce::Colour warningRed   { 0xffb84b40 };
    inline const juce::Colour signalCyan   { 0xff5eb7b0 };
    inline const juce::Colour ledOff       { 0xff2a2d30 };

    inline const juce::Colour knobBody     { 0xff1b1e22 };
    inline const juce::Colour knobBodyTop  { 0xff2a2e33 };
    inline const juce::Colour knobPointer  { 0xffe7e0d1 };
    inline const juce::Colour tick         { 0xff5b5f63 };
    inline const juce::Colour tickStrong   { 0xff8d9296 };

    // Rack materials (cues taken from the Apollo unit).
    inline const juce::Colour bakeliteLight { 0xff3a3e44 };
    inline const juce::Colour bakelite      { 0xff20242a };
    inline const juce::Colour bakeliteDark  { 0xff0c0e11 };
    inline const juce::Colour chromeLight   { 0xffd8d3c8 };
    inline const juce::Colour chromeMid     { 0xff8f897d };
    inline const juce::Colour chromeDark    { 0xff4a463f };
    inline const juce::Colour metalScrew    { 0xffb3ac9a };
    inline const juce::Colour metalHighlight{ 0xfff4f0e6 };
    inline const juce::Colour plateBlack    { 0xff0b0c0e };
    inline const juce::Colour plateBlackLit { 0xff1a1410 };
    inline const juce::Colour engraveShadow { 0xff000000 };
    inline const juce::Colour lampOff       { 0xff2a2d30 };

    inline const juce::Colour rackMetal     { 0xff1c1f22 };
    inline const juce::Colour rackMetalEdge { 0xff050607 };

    // -------------------------------------------------------------------------
    // Colour coding per section (used for markers / LEDs only, never fills)
    // -------------------------------------------------------------------------
    juce::Colour sectionAccent (const juce::String& section);

    // -------------------------------------------------------------------------
    // Metrics
    // -------------------------------------------------------------------------
    inline constexpr int   headerHeight      = 54;
    inline constexpr int   sectionBarHeight  = 36;
    inline constexpr int   keyboardHeight    = 72;
    inline constexpr int   minWidth          = 760;
    inline constexpr int   minHeight         = 500;
    inline constexpr int   defaultWidth      = 960;
    inline constexpr int   defaultHeight     = 620;
    inline constexpr int   maxWidth          = 1600;
    inline constexpr int   maxHeight         = 1000;

    inline constexpr float radiusSmall       = 2.0f;
    inline constexpr float radiusMedium      = 4.0f;
    inline constexpr float halfPi            = 1.5707964f;

    // -------------------------------------------------------------------------
    // Typography
    // -------------------------------------------------------------------------
    juce::Font brandFont();
    juce::Font moduleFont();
    juce::Font sectionFont();
    juce::Font labelFont();
    juce::Font legendFont();
    juce::Font valueFont();
    juce::Font presetFont();

    // -------------------------------------------------------------------------
    // Uniform UI scale. Set by the editor on resize so the physical detailing
    // (screws, engraving, lamps) scales with the window without rasterising.
    // -------------------------------------------------------------------------
    void setUiScale (float scale);
    float uiScale();
    inline float sc (float v) { return v * uiScale(); }

    // -------------------------------------------------------------------------
    // Material helpers (all vector, top-left light source).
    // -------------------------------------------------------------------------
    void drawBrushedPanel (juce::Graphics&, juce::Rectangle<float> bounds,
                           juce::Colour base, bool vertical);
    void drawRaisedPlate  (juce::Graphics&, juce::Rectangle<float> bounds, float corner);
    void drawInsetWell    (juce::Graphics&, juce::Rectangle<float> bounds, float corner,
                           float depth = 1.0f);
    void drawScrew        (juce::Graphics&, juce::Point<float> centre, float radius);
    void drawScrewsInCorners (juce::Graphics&, juce::Rectangle<float> bounds, float inset, float radius);
    void drawEngravedText (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                           const juce::Font& font, juce::Justification justification,
                           juce::Colour faceColour, juce::Colour highlightColour);
    void drawLamp         (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour,
                           float intensity, float corner);
}

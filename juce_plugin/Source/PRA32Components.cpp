#include "PRA32Components.h"
#include "PRA32ValueFormatter.h"
#include "PRA32LookAndFeel.h"

#include <cmath>

using namespace PRA32Theme;

namespace {

inline float angleFor (juce::Slider& s)
{
    const auto rp = s.getRotaryParameters();
    const auto prop = (float) s.valueToProportionOfLength (s.getValue());
    return rp.startAngleRadians + prop * (rp.endAngleRadians - rp.startAngleRadians);
}

inline juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + std::sin (a) * r, c.y - std::cos (a) * r };
}

// Fluted bakelite skirt + domed body + chrome step + ivory pointer + chrome cap.
// Directly cued from the Apollo rack unit.
void paintVintageKnob (juce::Graphics& g, juce::Point<float> centre, float radius,
                       float angle, bool dimmed, bool active)
{
    const float twoPi = juce::MathConstants<float>::twoPi;
    auto at = [centre] (float r, float a) { return polar (centre, r, a); };

    // Drop shadow on the panel.
    g.setColour (juce::Colours::black.withAlpha (dimmed ? 0.25f : 0.42f));
    g.fillEllipse (centre.x - radius, centre.y - radius + sc (2.5f), radius * 2.0f, radius * 2.0f);

    // Fluted bakelite skirt.
    {
        juce::ColourGradient skirt (bakeliteLight, centre.x - radius, centre.y - radius,
                                    bakeliteDark, centre.x + radius * 0.6f, centre.y + radius, false);
        g.setGradientFill (skirt);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        constexpr int flutes = 22;
        const float grooveInner = radius - juce::jmax (sc (4.5f), radius * 0.20f);
        const float grooveOuter = radius - sc (0.6f);

        for (int i = 0; i < flutes; ++i)
        {
            const float a = (float) i / (float) flutes * twoPi;
            g.setColour (juce::Colours::black.withAlpha (dimmed ? 0.16f : 0.34f));
            g.drawLine (juce::Line<float> (at (grooveInner, a), at (grooveOuter, a)), sc (1.9f));

            const float aRidge = a + 0.5f / (float) flutes * twoPi;
            g.setColour (juce::Colours::white.withAlpha (dimmed ? 0.03f : 0.08f));
            g.drawLine (juce::Line<float> (at (grooveInner + sc (0.5f), aRidge), at (grooveOuter, aRidge)),
                        sc (1.0f));
        }

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, sc (1.0f));
    }

    // Domed body.
    const float bodyR = radius * 0.74f;
    {
        juce::ColourGradient body (bakeliteLight.brighter (0.10f),
                                   centre.x - bodyR * 0.42f, centre.y - bodyR * 0.52f,
                                   bakeliteDark,
                                   centre.x + bodyR * 0.55f, centre.y + bodyR * 0.65f, false);
        g.setGradientFill (body);
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (dimmed ? 0.03f : 0.11f));
        g.fillEllipse (centre.x - bodyR * 0.60f, centre.y - bodyR * 0.82f,
                       bodyR * 1.05f, bodyR * 0.66f);

        g.setColour (chromeDark.withAlpha (dimmed ? 0.35f : 0.85f));
        g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, sc (1.3f));
        g.setColour (chromeLight.withAlpha (dimmed ? 0.10f : 0.30f));
        g.drawEllipse (centre.x - bodyR - sc (0.8f), centre.y - bodyR - sc (0.8f),
                       (bodyR + sc (0.8f)) * 2.0f, (bodyR + sc (0.8f)) * 2.0f, sc (0.8f));
    }

    // Ivory pointer running across the dome onto the skirt.
    {
        const auto p1 = at (radius * 0.20f, angle);
        const auto p2 = at (radius * 0.90f, angle);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine (juce::Line<float> (p1.translated (0.0f, sc (1.0f)), p2.translated (0.0f, sc (1.0f))),
                    sc (2.6f));

        g.setColour (dimmed ? textSecondary.withAlpha (0.45f)
                            : (active ? juce::Colours::white : knobPointer));
        g.drawLine (juce::Line<float> (p1, p2), sc (2.3f));

        g.setColour (dimmed ? textSecondary.withAlpha (0.4f) : knobPointer);
        g.fillEllipse (p2.x - sc (1.6f), p2.y - sc (1.6f), sc (3.2f), sc (3.2f));
    }

    // Chrome centre cap.
    {
        const float capR = bodyR * 0.30f;
        juce::ColourGradient cap (chromeLight, centre.x - capR * 0.45f, centre.y - capR * 0.55f,
                                  chromeDark, centre.x + capR * 0.55f, centre.y + capR * 0.60f, true);
        g.setGradientFill (cap);
        g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

        g.setColour (chromeDark.withAlpha (0.7f));
        g.drawEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f, sc (0.8f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (centre.x - capR * 0.45f, centre.y - capR * 0.55f, capR * 0.6f, capR * 0.4f);
    }
}

void paintKnobBody (juce::Graphics& g, juce::Rectangle<float> area, float angle,
                    float startAngle, float centreAngle, bool bipolar, juce::Colour accent,
                    bool enabled, bool active, int activeLedSegments, int totalLedSegments)
{
    auto centre = area.getCentre();
    const float outerR = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - sc (1.0f);
    const float knobR  = outerR * 0.70f;
    const float endAngle = 2.0f * centreAngle - startAngle;

    // Scale / serigraphy ticks belong to the panel, not to the knob body.
    const int ticks = 10;
    for (int i = 0; i <= ticks; ++i)
    {
        const float t = (float) i / (float) ticks;
        const float a = startAngle + t * (endAngle - startAngle);
        const bool major = (i % 5 == 0);
        const bool centreTick = bipolar && i == ticks / 2;

        float inner = knobR + sc (3.0f);
        float outer = outerR;
        float thick = major ? sc (1.7f) : sc (1.0f);

        if (major)              inner -= sc (4.5f);
        if (centreTick)       { inner = knobR - sc (1.0f); outer = outerR + sc (1.0f); thick = sc (1.8f); }

        juce::Colour col = centreTick ? accent.withAlpha (0.85f)
                                      : tickStrong.withAlpha (major ? 0.9f : 0.5f);
        if (! enabled) col = col.withAlpha (0.35f);

        g.setColour (col);
        g.drawLine (juce::Line<float> (polar (centre, inner, a), polar (centre, outer, a)), thick);
    }

    paintVintageKnob (g, centre, knobR, angle, ! enabled, active);

    // Optional annunciator row for stepped selectors.
    if (totalLedSegments > 1)
    {
        const float segW = juce::jmax (3.0f, (knobR * 2.0f) / (float) totalLedSegments - 2.0f);
        const float totalW = (segW + 2.0f) * (float) totalLedSegments - 2.0f;
        float x = centre.x - totalW * 0.5f;
        const float y = area.getBottom() - 3.0f;

        for (int i = 0; i < totalLedSegments; ++i)
        {
            const bool on = (i == activeLedSegments);
            g.setColour (on ? accent : ledOff);
            g.fillRoundedRectangle (x, y, segW, 2.5f, 1.0f);
            x += segW + 2.0f;
        }
    }
}

juce::String uppercase (const juce::String& s)
{
    return s.toUpperCase();
}

int bandCount (const juce::StringArray& labels)
{
    return juce::jmax (2, labels.size());
}

int bandIndexFor (const juce::StringArray& labels, int value)
{
    const int n = bandCount (labels);

    if (n == 2) return value < 64 ? 0 : 1;
    if (n == 3) return value < 32 ? 0 : (value < 96 ? 1 : 2);
    if (n == 6)
        return value < 13 ? 0 : (value < 39 ? 1 : (value < 64 ? 2
               : (value < 89 ? 3 : (value < 115 ? 4 : 5))));

    return juce::jlimit (0, n - 1, (value * n) / 128);
}

int bandRepresentative (const juce::StringArray& labels, int index)
{
    const int n = bandCount (labels);
    index = juce::jlimit (0, n - 1, index);

    if (n == 2) return index == 0 ? 0 : 127;
    if (n == 3) return index == 0 ? 0 : (index == 1 ? 64 : 127);

    if (n == 6)
    {
        static const int values[6] = { 6, 25, 51, 76, 101, 121 };
        return values[index];
    }

    return juce::jlimit (0, 127, (int) std::round ((index + 0.5) * 128.0 / n));
}

} // namespace

//==============================================================================
ParameterKnob::ParameterKnob (const SynthParamData& info,
                              juce::AudioProcessorValueTreeState& apvts)
    : param (info)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange ((double) param.min, (double) param.max, 1.0);
    setDoubleClickReturnValue (true, (double) param.def);
    setVelocityBasedMode (false);
    setWantsKeyboardFocus (true);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, param.id, *this);
}

void ParameterKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto nameArea = r.removeFromTop (11.0f);
    auto valueArea = r.removeFromBottom (13.0f);

    const bool enabled = isEnabled();
    const auto accent = sectionAccent (param.section);
    const bool bipolar = param.bipolar;
    const auto rp = getRotaryParameters();
    const float midAngle = (rp.startAngleRadians + rp.endAngleRadians) * 0.5f;

    paintKnobBody (g, r, angleFor (*this), rp.startAngleRadians, midAngle,
                   bipolar, accent, enabled, isMouseOverOrDragging(), -1, 0);

    drawEngravedText (g, uppercase (param.displayName), nameArea,
                      labelFont(), juce::Justification::centred, textSecondary,
                      engraveShadow.withAlpha (0.6f));

    g.setFont (valueFont());
    g.setColour (isMouseOverOrDragging() ? accent.brighter (0.2f) : textPrimary);
    g.drawFittedText (PRA32ValueFormatter::format (param, (int) std::lround (getValue())),
                      valueArea.toNearestInt(), juce::Justification::centred, 1);
}

//==============================================================================
SteppedSelector::SteppedSelector (const SynthParamData& info,
                                  juce::AudioProcessorValueTreeState& apvts)
    : param (info)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange ((double) param.min, (double) param.max, 1.0);
    setDoubleClickReturnValue (true, (double) param.def);
    setVelocityBasedMode (false);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, param.id, *this);
}

void SteppedSelector::setGridColumns (int columns)
{
    gridColumns = juce::jmax (0, columns);
    repaint();
}

int SteppedSelector::gridColumnsFor (int itemCount) const
{
    if (gridColumns > 0 && gridColumns < itemCount)
        return gridColumns;

    return juce::jmax (1, itemCount);
}

void SteppedSelector::selectFromPoint (juce::Point<int> position)
{
    if (! isEnabled())
        return;

    const int n = bandCount (param.enumLabels);
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (sc (12.0f));
    r.removeFromBottom (sc (13.0f));
    auto inner = r.reduced (1.0f, 2.0f).reduced (sc (2.0f));

    const int cols = gridColumnsFor (n);
    const int rows = (n + cols - 1) / cols;
    const float segW = inner.getWidth() / (float) cols;
    const float segH = inner.getHeight() / (float) rows;

    if (segW <= 0.0f || segH <= 0.0f)
        return;

    const int col = juce::jlimit (0, cols - 1, (int) std::floor ((position.x - inner.getX()) / segW));
    const int row = juce::jlimit (0, rows - 1, (int) std::floor ((position.y - inner.getY()) / segH));
    const int index = juce::jlimit (0, n - 1, row * cols + col);

    setValue (bandRepresentative (param.enumLabels, index), juce::sendNotificationSync);
}

void SteppedSelector::mouseDown (const juce::MouseEvent& e) { selectFromPoint (e.getPosition()); }
void SteppedSelector::mouseDrag (const juce::MouseEvent& e) { selectFromPoint (e.getPosition()); }

void SteppedSelector::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto nameArea = r.removeFromTop (sc (12.0f));
    auto valueArea = r.removeFromBottom (sc (13.0f));

    const auto accent = sectionAccent (param.section);
    const int n = bandCount (param.enumLabels);
    const int v = (int) std::lround (getValue());
    const int active = bandIndexFor (param.enumLabels, v);
    const int cols = gridColumnsFor (n);
    const int rows = (n + cols - 1) / cols;

    auto well = r.reduced (1.0f, 2.0f);
    drawInsetWell (g, well, sc (3.0f), 0.8f);

    auto inner = well.reduced (sc (2.0f));
    const float segW = inner.getWidth() / (float) cols;
    const float segH = inner.getHeight() / (float) rows;

    for (int i = 0; i < n; ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        juce::Rectangle<float> seg (inner.getX() + col * segW,
                                    inner.getY() + row * segH,
                                    segW - sc (1.0f), segH - sc (1.0f));
        const bool on = (i == active);

        if (on)
        {
            g.setColour (accent.withAlpha (isEnabled() ? 0.92f : 0.35f));
            g.fillRoundedRectangle (seg.reduced (sc (1.0f)), sc (3.0f));
            g.setColour (juce::Colours::black.withAlpha (0.30f));
            g.drawLine (seg.getX() + sc (3.0f), seg.getY() + sc (1.0f),
                        seg.getRight() - sc (3.0f), seg.getY() + sc (1.0f), sc (1.4f));
        }
        else
        {
            juce::ColourGradient key (panelRaised.brighter (0.10f), seg.getX(), seg.getY(),
                                      panelSunken, seg.getX(), seg.getBottom(), false);
            g.setGradientFill (key);
            g.fillRoundedRectangle (seg.reduced (sc (1.0f)), sc (3.0f));
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawLine (seg.getX() + sc (3.0f), seg.getY() + sc (1.5f),
                        seg.getRight() - sc (3.0f), seg.getY() + sc (1.5f), sc (1.0f));
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (seg.reduced (sc (1.0f)), sc (3.0f), sc (0.8f));
        }

        const auto label = param.enumLabels.isEmpty()
                               ? juce::String (i)
                               : param.enumLabels[juce::jlimit (0, param.enumLabels.size() - 1, i)];

        g.setFont (valueFont());
        g.setColour (on ? juce::Colours::black : (isEnabled() ? textPrimary : textDim));
        g.drawFittedText (label.toUpperCase(), seg.reduced (sc (2.0f)).toNearestInt(),
                          juce::Justification::centred, 1, 0.5f);
    }

    drawEngravedText (g, uppercase (param.displayName), nameArea,
                      labelFont(), juce::Justification::centred, textSecondary,
                      engraveShadow.withAlpha (0.6f));

    juce::ignoreUnused (valueArea);
}

//==============================================================================
ToggleSwitch::ToggleSwitch (const SynthParamData& info,
                            juce::AudioProcessorValueTreeState& apvts)
    : juce::Button (info.id), param (info)
{
    setClickingTogglesState (true);
    setToggleState ((double) param.def >= 64.0, juce::dontSendNotification);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, param.id, *this);
}

void ToggleSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    juce::ignoreUnused (highlighted);

    auto r = getLocalBounds().toFloat();
    auto nameArea = r.removeFromLeft (r.getWidth() * 0.55f);
    auto rocker = r.withSizeKeepingCentre (juce::jmin (sc (36.0f), r.getWidth() - sc (4.0f)), sc (18.0f));

    const bool on = getToggleState();
    const auto accent = sectionAccent (param.section);

    drawEngravedText (g, uppercase (param.displayName), nameArea,
                      labelFont(), juce::Justification::centredLeft,
                      on ? textPrimary : textSecondary, engraveShadow.withAlpha (0.6f));

    drawInsetWell (g, rocker, sc (4.0f), 0.8f);

    auto paddle = rocker.reduced (sc (2.5f));
    if (down)
        paddle = paddle.translated (0.0f, sc (1.0f));

    juce::ColourGradient grad (on ? accent : panelRaised.brighter (0.08f),
                               paddle.getX(), paddle.getY(),
                               on ? accent.darker (0.35f) : panelSunken,
                               paddle.getX(), paddle.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (paddle, sc (3.0f));

    g.setColour (juce::Colours::white.withAlpha (on ? 0.28f : 0.10f));
    g.drawLine (paddle.getX() + sc (3.0f), paddle.getY() + sc (1.0f),
                paddle.getRight() - sc (3.0f), paddle.getY() + sc (1.0f), sc (1.0f));

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (paddle, sc (3.0f), sc (1.0f));

    g.setColour (on ? juce::Colours::black : textSecondary);
    g.setFont (valueFont());
    g.drawText (on ? "ON" : "OFF", paddle.toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
ModulePanel::ModulePanel (juce::String t, juce::String sub, juce::Colour a)
    : title (std::move (t)), subsection (std::move (sub)), accent (a)
{
}

void ModulePanel::addControl (juce::Component& control)
{
    controls.add (&control);
    addAndMakeVisible (control);
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);

    drawRaisedPlate (g, b, radiusMedium);

    const float screwR = juce::jlimit (sc (2.4f), sc (3.6f), b.getHeight() * 0.018f);
    drawScrewsInCorners (g, b, sc (9.0f), screwR);

    auto header = getLocalBounds().reduced (juce::roundToInt (sc (16.0f)), juce::roundToInt (sc (6.0f)))
                      .removeFromTop (juce::jmax (14, juce::roundToInt (b.getHeight() * 0.12f)));

    // Section pilot lamp.
    const float lampSize = juce::jmin (sc (14.0f), (float) header.getHeight());
    auto lampArea = header.removeFromRight ((int) lampSize);
    lampArea = lampArea.withSizeKeepingCentre ((int) lampSize, (int) lampSize);
    header.removeFromRight (juce::roundToInt (sc (8.0f)));
    drawLamp (g, lampArea.toFloat(), accent, 0.75f, sc (2.5f));

    drawEngravedText (g, uppercase (title), header.toFloat(), moduleFont(),
                      juce::Justification::centredLeft, textPrimary,
                      engraveShadow.withAlpha (0.7f));

    if (subsection.isNotEmpty())
        drawEngravedText (g, uppercase (subsection), header.toFloat(), legendFont(),
                          juce::Justification::centredRight, accent,
                          engraveShadow.withAlpha (0.6f));

    const float lineY = (float) header.getBottom() + sc (3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawLine (sc (10.0f), lineY, (float) getWidth() - sc (10.0f), lineY, 1.0f);
}

void ModulePanel::resized()
{
    if (! autoLayout)
        return;

    auto area = getLocalBounds().reduced (6, 4);
    area.removeFromTop (16); // header

    const int n = controls.size();
    if (n == 0)
        return;

    int cols = juce::jmax (1, juce::jmin (columnsHint, n));
    if (getWidth() < 60 * cols)
        cols = juce::jmax (1, getWidth() / 60);

    const int rows = (n + cols - 1) / cols;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / juce::jmax (1, rows);

    for (int i = 0; i < n; ++i)
    {
        const int row = i / cols;
        const int col = i % cols;
        controls[i]->setBounds (area.getX() + col * cellW,
                                area.getY() + row * cellH,
                                cellW, cellH);
    }
}

//==============================================================================
SectionSelector::SectionSelector()
{
    setSections ({ "OSC", "FILTER", "ENVS", "MOD", "FX", "COLOR" });
}

void SectionSelector::setSections (const juce::StringArray& names)
{
    sections = names;
    active = juce::jlimit (0, juce::jmax (0, sections.size() - 1), active);
    repaint();
}

void SectionSelector::setActive (int index, juce::NotificationType notification)
{
    index = juce::jlimit (0, juce::jmax (0, sections.size() - 1), index);

    if (index == active)
        return;

    active = index;
    repaint();

    if (notification != juce::dontSendNotification && onSectionChanged != nullptr)
        onSectionChanged (active);
}

juce::Rectangle<int> SectionSelector::boundsFor (int index) const
{
    const int n = juce::jmax (1, sections.size());
    const int w = getWidth() / n;
    return { index * w, 0, w, getHeight() };
}

void SectionSelector::paint (juce::Graphics& g)
{
    for (int i = 0; i < sections.size(); ++i)
    {
        auto r = boundsFor (i).toFloat().reduced (sc (2.0f), sc (3.0f));
        const bool isActive = (i == active);
        const bool isHover = (i == hover);
        const auto accent = sectionAccent (sections[i]);

        if (isActive)
        {
            drawInsetWell (g, r, sc (3.0f), 1.0f);
            g.setColour (accent.withAlpha (0.18f));
            g.fillRoundedRectangle (r.reduced (sc (1.5f)), sc (2.5f));
        }
        else
        {
            juce::ColourGradient key (panelRaised.brighter (0.10f), r.getX(), r.getY(),
                                      panelSunken, r.getX(), r.getBottom(), false);
            g.setGradientFill (key);
            g.fillRoundedRectangle (r, sc (3.0f));
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawLine (r.getX() + sc (3.0f), r.getY() + sc (1.0f),
                        r.getRight() - sc (3.0f), r.getY() + sc (1.0f), sc (1.0f));
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawLine (r.getX() + sc (3.0f), r.getBottom() - sc (1.0f),
                        r.getRight() - sc (3.0f), r.getBottom() - sc (1.0f), sc (1.4f));
            g.setColour (isHover ? border.brighter (0.25f) : border);
            g.drawRoundedRectangle (r.reduced (0.5f), sc (3.0f), sc (1.0f));
        }

        // Annunciator lamp.
        const float lampSize = sc (8.0f);
        auto lamp = juce::Rectangle<float> (lampSize, lampSize)
                        .withCentre ({ r.getX() + sc (11.0f), r.getCentreY() });
        drawLamp (g, lamp, accent, isActive ? 1.0f : 0.0f, sc (2.0f));

        auto textArea = r.toNearestInt();
        textArea.removeFromLeft (juce::roundToInt (sc (20.0f)));

        drawEngravedText (g, sections[i], textArea.toFloat(), sectionFont(),
                          juce::Justification::centredLeft,
                          isActive ? textPrimary : (isHover ? textSecondary.brighter (0.2f) : textSecondary),
                          engraveShadow.withAlpha (0.65f));
    }
}

void SectionSelector::resized() {}

void SectionSelector::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < sections.size(); ++i)
        if (boundsFor (i).contains (e.getPosition()))
        {
            setActive (i);
            break;
        }
}

void SectionSelector::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < sections.size(); ++i)
        if (boundsFor (i).contains (e.getPosition()))
            h = i;

    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void SectionSelector::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1)
    {
        hover = -1;
        repaint();
    }
}

//==============================================================================
//==============================================================================
EnvelopePanel::EnvelopePanel (juce::AudioProcessorValueTreeState& apvts,
                              const SynthParamData& attack, const SynthParamData& decay,
                              const SynthParamData& sustain, const SynthParamData& release,
                              juce::Colour acc)
    : state (apvts), params { attack, decay, sustain, release }, accent (acc)
{
    for (int i = 0; i < 4; ++i)
    {
        auto* knob = knobs.add (new ParameterKnob (params[i], apvts));
        addAndMakeVisible (knob);
        state.addParameterListener (params[i].id, this);

        if (auto* value = state.getRawParameterValue (params[i].id))
            props[i] = PRA32ValueFormatter::curveProportion (params[i],
                                                             (int) value->load());
    }
}

EnvelopePanel::~EnvelopePanel()
{
    for (const auto& p : params)
        state.removeParameterListener (p.id, this);
}

void EnvelopePanel::parameterChanged (const juce::String& id, float newValue)
{
    for (int i = 0; i < 4; ++i)
        if (params[i].id == id)
            props[i] = PRA32ValueFormatter::curveProportion (params[i], (int) newValue);

    repaint();
}

void EnvelopePanel::paint (juce::Graphics& g)
{
    drawInsetWell (g, curveArea.toFloat(), sc (3.0f), 0.9f);

    auto a = curveArea.toFloat().reduced (10.0f, 8.0f);
    const float x0 = a.getX();
    const float x1 = a.getRight();
    const float base = a.getBottom();
    const float top = a.getY();
    const float spanY = base - top;
    const float spanX = x1 - x0;

    const float ax = x0 + props[0] * spanX * 0.35f;
    const float dx = ax + props[1] * spanX * 0.25f;
    const float susY = base - props[2] * spanY;
    const float rx = juce::jmin (x1, dx + props[3] * spanX * 0.40f + 6.0f);

    juce::Path p;
    p.startNewSubPath (x0, base);
    p.lineTo (ax, top);
    p.lineTo (dx, susY);
    p.lineTo (rx, base);

    g.setColour (accent.withAlpha (0.85f));
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
}

void EnvelopePanel::resized()
{
    auto r = getLocalBounds().reduced (4);

    if (r.getHeight() > 90)
    {
        curveArea = r.removeFromTop (juce::jmax (46, r.getHeight() * 42 / 100));
        r.removeFromTop (2);
    }
    else
    {
        curveArea = {};
    }

    const int cw = r.getWidth() / 4;

    for (int i = 0; i < 4; ++i)
        knobs[i]->setBounds (r.getX() + i * cw, r.getY(), cw, r.getHeight());
}

//==============================================================================
FilterResponseGraph::FilterResponseGraph (juce::AudioProcessorValueTreeState& apvts,
                                          juce::Colour acc)
    : state (apvts), accent (acc)
{
    state.addParameterListener ("filterCutoff", this);
    state.addParameterListener ("filterReso", this);
    state.addParameterListener ("filterMode", this);

    if (auto* v = state.getRawParameterValue ("filterCutoff")) cutoffValue = v->load();
    if (auto* v = state.getRawParameterValue ("filterReso"))   resoValue = v->load();
    if (auto* v = state.getRawParameterValue ("filterMode"))   modeValue = v->load();
}

FilterResponseGraph::~FilterResponseGraph()
{
    state.removeParameterListener ("filterCutoff", this);
    state.removeParameterListener ("filterReso", this);
    state.removeParameterListener ("filterMode", this);
}

void FilterResponseGraph::parameterChanged (const juce::String& id, float newValue)
{
    if (id == "filterCutoff")     cutoffValue = newValue;
    else if (id == "filterReso")  resoValue = newValue;
    else if (id == "filterMode")  modeValue = newValue;

    repaint();
}

void FilterResponseGraph::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    drawInsetWell (g, area, sc (3.0f), 0.9f);

    g.setFont (legendFont());
    g.setColour (textDim);
    g.drawText ("RESPONSE", getLocalBounds().reduced (8, 4).removeFromTop (10),
                juce::Justification::topLeft);

    auto a = area.reduced (9.0f, 9.0f);

    const double fc = PRA32ValueFormatter::cutoffHz ((int) std::lround (cutoffValue));
    const double q = juce::jmax (0.7, PRA32ValueFormatter::resonanceQ ((int) std::lround (resoValue)));
    const bool highpass = modeValue >= 64.0f;

    const double fMin = 20.0, fMax = 20000.0;
    const float base = a.getBottom();
    const float top = a.getY();
    const float spanY = base - top;
    const float spanX = a.getWidth();

    juce::Path curve;

    for (int i = 0; i <= 120; ++i)
    {
        const double t = i / 120.0;
        const double f = fMin * std::pow (fMax / fMin, t);
        const double w = f / fc;
        const double denom = std::sqrt (std::pow (1.0 - w * w, 2.0) + std::pow (w / q, 2.0));
        const double mag = (denom > 1.0e-9) ? ((highpass ? w * w : 1.0) / denom) : 0.0;

        const float norm = (float) juce::jlimit (0.0, 1.0, mag / 2.0);
        const float x = a.getX() + (float) t * spanX;
        const float y = base - norm * spanY;

        if (i == 0) curve.startNewSubPath (x, y);
        else        curve.lineTo (x, y);
    }

    g.setColour (accent.withAlpha (0.85f));
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

    g.setColour (separator);
    g.drawLine (a.getX(), base, a.getRight(), base, 1.0f);
}

//==============================================================================
PRA32Keyboard::PRA32Keyboard (juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard)
{
}

void PRA32Keyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g,
                                   juce::Rectangle<float> area, bool isDown, bool isOver,
                                   juce::Colour, juce::Colour)
{
    auto r = area.reduced (0.5f);

    juce::ColourGradient grad (juce::Colour (0xfff2efe6), r.getX(), r.getY(),
                               juce::Colour (0xffcfc9ba), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 1.5f);

    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.drawRoundedRectangle (r, 1.5f, 0.8f);

    if (isDown)
    {
        g.setColour (amber.withAlpha (0.55f));
        g.fillRoundedRectangle (r.reduced (1.0f), 1.5f);
    }
    else if (isOver)
    {
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillRoundedRectangle (r.reduced (1.0f), 1.5f);
    }

    if (midiNoteNumber % 12 == 0)
    {
        g.setColour (juce::Colour (0xff6b665c));
        g.setFont (legendFont());
        g.drawText (juce::MidiMessage::getMidiNoteName (midiNoteNumber, true, true, 3),
                    r.removeFromBottom (12.0f), juce::Justification::centred, false);
    }
}

void PRA32Keyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area,
                                   bool isDown, bool isOver, juce::Colour)
{
    auto r = area.reduced (0.5f);

    juce::ColourGradient grad (juce::Colour (0xff2a2e33), r.getX(), r.getY(),
                               juce::Colour (0xff05070a), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (r.getX() + 2.0f, r.getY() + 1.5f, r.getRight() - 2.0f, r.getY() + 1.5f, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 2.0f, 0.8f);

    if (isDown)
    {
        g.setColour (amber.withAlpha (0.6f));
        g.fillRoundedRectangle (r.reduced (1.0f), 2.0f);
    }
    else if (isOver)
    {
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (r.reduced (1.0f), 2.0f);
    }
}

//==============================================================================
ValueReadout::ValueReadout()
{
    setInterceptsMouseClicks (true, false);
}

void ValueReadout::mouseDown (const juce::MouseEvent&)
{
    if (onClick != nullptr)
        onClick();
}

void ValueReadout::setText (const juce::String& p, const juce::String& s)
{
    primary = p;
    secondary = s;
    repaint();
}

void ValueReadout::setColours (juce::Colour p, juce::Colour s)
{
    primaryColour = p;
    secondaryColour = s;
    repaint();
}

void ValueReadout::paint (juce::Graphics& g)
{
    drawInsetWell (g, getLocalBounds().toFloat(), sc (3.0f), 0.9f);

    auto r = getLocalBounds().reduced (juce::roundToInt (sc (6.0f)), juce::roundToInt (sc (2.0f)));

    if (secondary.isEmpty())
    {
        g.setFont (primaryFont);
        g.setColour (primaryColour);
        g.drawFittedText (primary, r, justification, 1);
        return;
    }

    auto main = r.removeFromTop (juce::jmax (12, r.getHeight() * 3 / 5));
    g.setFont (primaryFont);
    g.setColour (primaryColour);
    g.drawFittedText (primary, main, justification, 1);

    g.setFont (legendFont());
    g.setColour (secondaryColour);
    g.drawFittedText (secondary, r, justification, 1);
}

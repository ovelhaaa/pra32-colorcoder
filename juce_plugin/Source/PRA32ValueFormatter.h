#pragma once

#include <JuceHeader.h>
#include "SynthParameters.h"

// Presentation-only conversion of a raw 0..127 engine value into a musical /
// engineering readout. Never used to compute the value sent to the engine.
class PRA32ValueFormatter
{
public:
    static juce::String format (const SynthParamData& param, int value);

    // Short popup text used by knobs while dragging.
    static juce::String formatShort (const SynthParamData& param, int value);

    // Engine-grounded values reused by the UI-only graphs.
    static double cutoffHz (int value);
    static double resonanceQ (int value);

    // 0..1 proportion for envelope curve drawing (log-scaled for times).
    static float curveProportion (const SynthParamData& param, int value);
};

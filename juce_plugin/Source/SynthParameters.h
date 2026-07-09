#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

struct SynthParamData {
    juce::String id;
    juce::String label;
    int cc;
    int min;
    int max;
    int def;
    juce::String tab;
    juce::String type;
};

class SynthParameters {
public:
    static std::vector<SynthParamData> getParameters();
};

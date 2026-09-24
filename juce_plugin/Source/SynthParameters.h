#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include "PRA32ParamLogic.h"

// -----------------------------------------------------------------------------
// Control kinds / display formats are presentation-only metadata. They never
// change parameter meaning, range or the value sent to the synth engine.
// -----------------------------------------------------------------------------

enum class PRA32ControlKind {
    Rotary,          // continuous, unipolar
    BipolarRotary,   // continuous, explicit centre
    SteppedSelector, // discrete positions with labels
    Toggle,          // two-state switch
    Fader            // vertical mini fader
};

enum class PRA32FormatKind {
    Integer,
    Hertz,
    HertzOrKilo,
    Milliseconds,
    MillisecondsOrSeconds,
    MillisecondsNoDecay,
    Semitones,
    Detune,
    ResonanceQ,
    Percent,
    Decibels,
    SignedAmount,
    KeyTrack,
    Pan,
    PitchModAmount,
    OscMix,
    NoiseSub,
    Portamento,
    DelayTime,
    ChorusDepth,
    Enum,
    OnOff
};

struct SynthParamData {
    juce::String id;         // stable APVTS id (never renamed for visuals)
    juce::String label;      // host-facing parameter name
    juce::String displayName;// panel serigraphy label
    juce::String presetKey;  // factory preset JSON key
    int cc;
    int min;
    int max;
    int def;
    juce::String section;    // OSC / FILTER / ENVS / MOD / FX / CHARACTER
    juce::String subsection; // module inside the page
    PRA32ControlKind kind;
    bool bipolar;
    int centre;
    PRA32FormatKind format;
    juce::String unit;
    juce::StringArray enumLabels;
    int visualWeight;        // 1 = auxiliary, 2 = normal, 3 = hero
};

class SynthParameters {
public:
    static const std::vector<SynthParamData>& getParameters();
    static const SynthParamData* find(const juce::String& id);

    // --- Enum metadata -------------------------------------------------------
    // The engine interprets several stepped selectors as non-uniform CC bands.
    // These helpers are the single source of truth for the mapping between a
    // raw 0..127 value, the displayed choice index, and the representative CC
    // value that the UI writes back. Params without explicit ranges fall back
    // to the generic uniform banding used by simple 2/3-state selectors.
    static const PRA32ParamLogic::EnumRange* enumRanges (const SynthParamData& p, int& count);
    static int enumIndex (const SynthParamData& p, int value);
    static int enumRepresentative (const SynthParamData& p, int index);

    // CC values come from the engine constants (pra32-u2-constants.h).
    enum Cc {
        ccOsc1Wave    = 102, ccOsc1Shape = 19,  ccOsc1Morph = 20,
        ccOsc2Wave    = 104, ccOsc2Coarse = 85, ccOsc2Pitch = 76,
        ccOscMix      = 21,  ccSubOsc = 23,     ccOscDrift = 82,
        ccSawWMode    = 83,
        ccFilterCutoff = 74, ccFilterReso = 71, ccFilterMode = 39,
        ccFilterEgAmt = 24,  ccFilterKeyTrk = 9, ccBreathFltAmt = 60,
        ccRelEqDecay  = 105,
        ccEgAttack = 73, ccEgDecay = 75, ccEgSustain = 30, ccEgRelease = 72,
        ccEgOscAmt = 89, ccEgOscDst = 8,
        ccAmpAttack = 52, ccAmpDecay = 53, ccAmpSustain = 54, ccAmpRelease = 55,
        ccLfoWave = 33, ccLfoRate = 3, ccLfoFade = 56, ccLfoDepth = 17,
        ccLfoFltAmt = 25, ccLfoOscAmt = 13, ccLfoOscDst = 103,
        ccPbRange = 57, ccPortamento = 5,
        ccChorusMix = 93, ccChorusRate = 58, ccChorusDepth = 59,
        ccDelayTime = 90, ccDelayLevel = 94, ccDelayFeedback = 92,
        ccDelayMode = 35,
        ccPan = 10, ccAmpGain = 15, ccEgAmpMod = 36,
        ccVoiceMode = 18, ccVoiceAsgnMode = 110,
        ccBthAmpMod = 61, ccEgVelSens = 62, ccAmpVelSens = 63,
        ccAftTlfoAmt = 109
    };
};

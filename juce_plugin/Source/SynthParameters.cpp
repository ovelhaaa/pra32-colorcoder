#include "SynthParameters.h"

std::vector<SynthParamData> SynthParameters::getParameters() {
    return {
        // OSC
        { "osc1Wave", "OSC 1 WAVE", 24, 0, 127, 0, "OSC", "h" },
        { "osc1Shape", "OSC 1 SHAPE", 70, 0, 127, 0, "OSC", "h" },
        { "osc1Morph", "OSC 1 MORPH", 104, 0, 127, 0, "OSC", "h" },
        { "osc2Wave", "OSC 2 WAVE", 25, 0, 127, 0, "OSC", "h" },
        { "osc2Coarse", "O2 COARSE", 20, 0, 127, 64, "OSC", "h" },
        { "osc2Pitch", "O2 FINE", 21, 0, 127, 64, "OSC", "h" },
        { "oscMix", "OSC 1/2 MIX", 22, 0, 127, 64, "OSC", "v" },
        { "subOsc", "SUB/NOISE", 23, 0, 127, 0, "OSC", "v" },
        { "oscDrift", "ANALOG DRIFT", 26, 0, 127, 0, "OSC", "h" },
        { "sawWMode", "SAW MODE", 27, 0, 127, 0, "OSC", "h" },
        // FILTER
        { "filterCutoff", "CUTOFF FREQ", 74, 0, 127, 127, "FILTER", "h" },
        { "filterReso", "RESONANCE", 71, 0, 127, 0, "FILTER", "h" },
        { "filterMode", "TYPE (LP/HP)", 28, 0, 127, 0, "FILTER", "h" },
        { "egFltAmt", "MOD AMOUNT", 29, 0, 127, 64, "FILTER", "h" },
        { "filterKeyTrk", "KEY TRACK", 30, 0, 127, 64, "FILTER", "h" },
        { "bthFltAmt", "BREATH AMT", 31, 0, 127, 64, "FILTER", "h" },
        { "relEqDcy", "REL = DEC", 88, 0, 127, 0, "FILTER", "h" },
        // ENVS
        { "egOscAmt", "PITCH MOD", 105, 0, 127, 64, "ENVS", "h" },
        { "egAttack", "MOD ATTACK", 73, 0, 127, 0, "ENVS", "v" },
        { "egDecay", "MOD DECAY", 75, 0, 127, 64, "ENVS", "v" },
        { "egSustain", "MOD SUSTAIN", 33, 0, 127, 127, "ENVS", "v" },
        { "egRelease", "MOD RELEASE", 72, 0, 127, 64, "ENVS", "v" },
        { "ampAttack", "AMP ATTACK", 106, 0, 127, 0, "ENVS", "v" },
        { "ampDecay", "AMP DECAY", 107, 0, 127, 64, "ENVS", "v" },
        { "ampSustain", "AMP SUSTAIN", 108, 0, 127, 127, "ENVS", "v" },
        { "ampRelease", "AMP RELEASE", 109, 0, 127, 64, "ENVS", "v" },
        // MOD
        { "lfoWave", "LFO WAVE", 14, 0, 127, 0, "MOD", "h" },
        { "lfoRate", "LFO RATE", 76, 0, 127, 64, "MOD", "h" },
        { "lfoFltAmt", "LFO CUTOFF", 15, 0, 127, 64, "MOD", "h" },
        { "lfoOscAmt", "LFO PITCH", 16, 0, 127, 64, "MOD", "h" },
        { "lfoFadeTime", "LFO FADE IN", 17, 0, 127, 0, "MOD", "h" },
        { "pbRange", "BEND RANGE", 85, 0, 127, 2, "MOD", "h" },
        { "coarseTune", "MASTER COARSE", 86, 0, 127, 64, "MOD", "h" },
        { "fineTune", "MASTER FINE", 87, 0, 127, 64, "MOD", "h" },
        { "portaTime", "GLIDE TIME", 5, 0, 127, 0, "MOD", "h" },
        { "modRate", "VIBRATO RATE", 110, 0, 127, 64, "MOD", "h" },
        { "modDepth", "VIBRATO DEPTH", 111, 0, 127, 0, "MOD", "h" },
        // FX
        { "choRate", "CHORUS RATE", 93, 0, 127, 0, "FX", "h" },
        { "choDepth", "CHORUS DEPTH", 114, 0, 127, 0, "FX", "h" },
        { "delayTime", "DELAY TIME", 112, 0, 127, 64, "FX", "h" },
        { "delayDepth", "DELAY LEVEL", 113, 0, 127, 0, "FX", "h" },
        { "pan", "PANNING", 10, 0, 127, 64, "FX", "h" },
        { "volume", "MASTER VOL", 7, 0, 127, 100, "FX", "v" },
        { "ampExpnt", "EG AMP MOD", 116, 0, 127, 0, "FX", "h" },
        { "ampGain", "AMP GAIN", 117, 0, 127, 64, "FX", "h" },
        { "pannerType", "PAN TYPE", 118, 0, 127, 0, "FX", "h" },
        { "choType", "CHORUS TYPE", 119, 0, 127, 0, "FX", "h" },
        // LO-FI
        { "lfoWaveForm", "LFO FORM", 89, 0, 127, 0, "LO-FI", "h" },
        { "portaMode", "VOICE MODE", 90, 0, 127, 0, "LO-FI", "h" },
        { "pwmRate", "PWM RATE", 115, 0, 127, 64, "LO-FI", "h" },
        { "lfoSync", "LFO SYNC", 120, 0, 127, 0, "LO-FI", "h" },
        { "noiseMode", "NOISE MODE", 121, 0, 127, 0, "LO-FI", "h" },
        { "distMode", "DIST MODE", 122, 0, 127, 0, "LO-FI", "h" },
        { "bitCrush", "BITCRUSH", 123, 0, 127, 0, "LO-FI", "h" },
        { "vcfType", "VCF TYPE", 124, 0, 127, 0, "LO-FI", "h" }
    };
}

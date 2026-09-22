#include "SynthParameters.h"

namespace {

juce::StringArray enumOsc1Wave()   { return { "SAW", "SQR", "TRI", "SIN", "WT", "PLS" }; }
juce::StringArray enumOsc2Wave()   { return { "SAW", "SQR", "TRI", "SIN", "OSC 1", "NOISE" }; }
juce::StringArray enumLfoWave()    { return { "TRI", "SINE", "NOISE", "SAW", "S&H", "SQUARE" }; }
juce::StringArray enumFilterMode() { return { "LOW PASS", "HIGH PASS" }; }
juce::StringArray enumSawMode()    { return { "STRAIGHT", "CURVED" }; }
juce::StringArray enumModDst()     { return { "PITCH 1+2", "PITCH 1+2", "PITCH 2", "PITCH 2", "CUTOFF", "SHAPE 1" }; }
juce::StringArray enumVoiceMode()  { return { "POLY", "POLY", "MONO", "MONO", "LGTO PORTA", "LEGATO" }; }
juce::StringArray enumAsgnMode()   { return { "MODE 1", "MODE 2" }; }
juce::StringArray enumBreathAmp()  { return { "OFF", "QUAD", "LIN" }; }
juce::StringArray enumDelayMode()  { return { "STEREO", "PING PONG" }; }
juce::StringArray enumOnOff()      { return { "OFF", "ON" }; }

} // namespace

const std::vector<SynthParamData>& SynthParameters::getParameters()
{
    static const std::vector<SynthParamData> parameters = {
        // ---------------------------------------------------------------------
        // OSC
        // ---------------------------------------------------------------------
        { "osc1Wave", "Osc 1 Wave", "WAVE", "OSC_1_WAVE", ccOsc1Wave, 0, 127, 0,
          "OSC", "OSCILLATOR 1", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumOsc1Wave(), 2 },
        { "osc1Shape", "Osc 1 Shape", "SHAPE", "OSC_1_SHAPE", ccOsc1Shape, 0, 127, 0,
          "OSC", "OSCILLATOR 1", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "osc1Morph", "Osc 1 Morph", "MORPH", "OSC_1_MORPH", ccOsc1Morph, 0, 127, 0,
          "OSC", "OSCILLATOR 1", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "oscDrift", "Osc Drift", "DRIFT", "OSC_DRIFT", ccOscDrift, 0, 127, 0,
          "OSC", "OSCILLATOR 1", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 1 },
        { "sawWMode", "Saw Wave Mode", "SAW MODE", "OSC_SAW_W_MODE", ccSawWMode, 0, 127, 0,
          "OSC", "OSCILLATOR 1", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumSawMode(), 1 },

        { "osc2Wave", "Osc 2 Wave", "WAVE", "OSC_2_WAVE", ccOsc2Wave, 0, 127, 0,
          "OSC", "OSCILLATOR 2", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumOsc2Wave(), 2 },
        { "osc2Coarse", "Osc 2 Coarse", "COARSE", "OSC_2_COARSE", ccOsc2Coarse, 0, 127, 64,
          "OSC", "OSCILLATOR 2", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::Semitones, "st", {}, 2 },
        { "osc2Pitch", "Osc 2 Fine", "FINE", "OSC_2_PITCH", ccOsc2Pitch, 0, 127, 64,
          "OSC", "OSCILLATOR 2", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::Detune, "", {}, 2 },

        { "oscMix", "Osc Mix", "OSC 1 / OSC 2", "MIXER_OSC_MIX", ccOscMix, 0, 127, 64,
          "OSC", "MIXER", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::OscMix, "", {}, 2 },
        { "subOsc", "Noise / Sub Osc", "NOISE / SUB", "MIXER_SUB_OSC", ccSubOsc, 0, 127, 0,
          "OSC", "MIXER", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::NoiseSub, "", {}, 2 },

        // ---------------------------------------------------------------------
        // FILTER
        // ---------------------------------------------------------------------
        { "filterCutoff", "Filter Cutoff", "CUTOFF", "FILTER_CUTOFF", ccFilterCutoff, 0, 127, 127,
          "FILTER", "FILTER", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::HertzOrKilo, "Hz", {}, 3 },
        { "filterReso", "Filter Resonance", "RESONANCE", "FILTER_RESO", ccFilterReso, 0, 127, 0,
          "FILTER", "FILTER", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::ResonanceQ, "Q", {}, 3 },
        { "filterMode", "Filter Mode", "FILTER TYPE", "FILTER_MODE", ccFilterMode, 0, 127, 0,
          "FILTER", "FILTER", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumFilterMode(), 2 },
        { "egFltAmt", "Filter EG Amount", "MOD AMOUNT", "FILTER_EG_AMT", ccFilterEgAmt, 0, 127, 64,
          "FILTER", "FILTER", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::SignedAmount, "", {}, 2 },
        { "filterKeyTrk", "Filter Key Track", "KEY TRACK", "FILTER_KEY_TRK", ccFilterKeyTrk, 0, 127, 64,
          "FILTER", "FILTER", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::KeyTrack, "", {}, 1 },
        { "bthFltAmt", "Breath Filter Amount", "BREATH AMT", "BTH_FILTER_AMT", ccBreathFltAmt, 0, 127, 64,
          "FILTER", "FILTER", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::SignedAmount, "", {}, 1 },
        { "relEqDcy", "Release Equals Decay", "REL = DEC", "REL_EQ_DECAY", ccRelEqDecay, 0, 127, 0,
          "FILTER", "FILTER", PRA32ControlKind::Toggle, false, 0,
          PRA32FormatKind::OnOff, "", enumOnOff(), 1 },

        // ---------------------------------------------------------------------
        // ENVS
        // ---------------------------------------------------------------------
        { "egAttack", "Mod Attack", "ATTACK", "EG_ATTACK", ccEgAttack, 0, 127, 0,
          "ENVS", "MOD ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Milliseconds, "ms", {}, 2 },
        { "egDecay", "Mod Decay", "DECAY", "EG_DECAY", ccEgDecay, 0, 127, 64,
          "ENVS", "MOD ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::MillisecondsNoDecay, "ms", {}, 2 },
        { "egSustain", "Mod Sustain", "SUSTAIN", "EG_SUSTAIN", ccEgSustain, 0, 127, 127,
          "ENVS", "MOD ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "egRelease", "Mod Release", "RELEASE", "EG_RELEASE", ccEgRelease, 0, 127, 64,
          "ENVS", "MOD ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Milliseconds, "ms", {}, 2 },
        { "ampAttack", "Amp Attack", "ATTACK", "AMP_ATTACK", ccAmpAttack, 0, 127, 0,
          "ENVS", "AMP ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Milliseconds, "ms", {}, 2 },
        { "ampDecay", "Amp Decay", "DECAY", "AMP_DECAY", ccAmpDecay, 0, 127, 64,
          "ENVS", "AMP ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::MillisecondsNoDecay, "ms", {}, 2 },
        { "ampSustain", "Amp Sustain", "SUSTAIN", "AMP_SUSTAIN", ccAmpSustain, 0, 127, 127,
          "ENVS", "AMP ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "ampRelease", "Amp Release", "RELEASE", "AMP_RELEASE", ccAmpRelease, 0, 127, 64,
          "ENVS", "AMP ENVELOPE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Milliseconds, "ms", {}, 2 },
        { "egOscAmt", "EG Pitch Amount", "PITCH MOD", "EG_OSC_AMT", ccEgOscAmt, 0, 127, 64,
          "CHARACTER", "PITCH MOD", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::PitchModAmount, "", {}, 2 },
        { "egOscDst", "EG Mod Destination", "EG DEST", "EG_OSC_DST", ccEgOscDst, 0, 127, 0,
          "CHARACTER", "PITCH MOD", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumModDst(), 1 },

        // ---------------------------------------------------------------------
        // MOD
        // ---------------------------------------------------------------------
        { "lfoWave", "LFO Wave", "WAVE", "LFO_WAVE", ccLfoWave, 0, 127, 0,
          "MOD", "LFO", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumLfoWave(), 2 },
        { "lfoRate", "LFO Rate", "RATE", "LFO_RATE", ccLfoRate, 0, 127, 64,
          "MOD", "LFO", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Hertz, "Hz", {}, 2 },
        { "lfoFadeTime", "LFO Fade Time", "FADE IN", "LFO_FADE_TIME", ccLfoFade, 0, 127, 0,
          "MOD", "LFO", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::MillisecondsOrSeconds, "ms", {}, 1 },
        { "lfoDepth", "LFO Depth", "DEPTH", "LFO_DEPTH", ccLfoDepth, 0, 127, 0,
          "MOD", "LFO", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "lfoFltAmt", "LFO Filter Amount", "CUTOFF AMT", "LFO_FILTER_AMT", ccLfoFltAmt, 0, 127, 64,
          "MOD", "LFO", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::SignedAmount, "", {}, 2 },
        { "lfoOscAmt", "LFO Pitch Amount", "PITCH AMT", "LFO_OSC_AMT", ccLfoOscAmt, 0, 127, 64,
          "MOD", "LFO", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::PitchModAmount, "", {}, 2 },
        { "lfoOscDst", "LFO Mod Destination", "LFO DEST", "LFO_OSC_DST", ccLfoOscDst, 0, 127, 0,
          "MOD", "LFO", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumModDst(), 1 },
        { "pbRange", "Pitch Bend Range", "BEND RANGE", "P_BEND_RANGE", ccPbRange, 0, 127, 2,
          "CHARACTER", "PERFORMANCE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Semitones, "st", {}, 1 },
        { "portaTime", "Portamento", "GLIDE TIME", "PORTAMENTO", ccPortamento, 0, 127, 0,
          "CHARACTER", "PERFORMANCE", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Portamento, "ms", {}, 1 },

        // ---------------------------------------------------------------------
        // FX
        // ---------------------------------------------------------------------
        { "chorusMix", "Chorus Level", "LEVEL", "CHORUS_MIX", ccChorusMix, 0, 127, 127,
          "FX", "CHORUS", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "choRate", "Chorus Rate", "RATE", "CHORUS_RATE", ccChorusRate, 0, 127, 0,
          "FX", "CHORUS", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Hertz, "Hz", {}, 2 },
        { "choDepth", "Chorus Depth", "DEPTH", "CHORUS_DEPTH", ccChorusDepth, 0, 127, 0,
          "FX", "CHORUS", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::ChorusDepth, "ms", {}, 2 },
        { "delayTime", "Delay Time", "TIME", "DELAY_TIME", ccDelayTime, 0, 127, 64,
          "FX", "DELAY", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::DelayTime, "ms", {}, 3 },
        { "delayDepth", "Delay Level", "LEVEL", "DELAY_LEVEL", ccDelayLevel, 0, 127, 0,
          "FX", "DELAY", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "delayFeedback", "Delay Feedback", "FEEDBACK", "DELAY_FEEDBACK", ccDelayFeedback, 0, 127, 64,
          "FX", "DELAY", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 2 },
        { "delayMode", "Delay Mode", "MODE", "DELAY_MODE", ccDelayMode, 0, 127, 0,
          "FX", "DELAY", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumDelayMode(), 1 },
        { "pan", "Pan", "PAN", "PAN", ccPan, 0, 127, 64,
          "CHARACTER", "OUTPUT", PRA32ControlKind::BipolarRotary, true, 64,
          PRA32FormatKind::Pan, "", {}, 2 },
        { "ampGain", "Amp Gain", "AMP GAIN", "AMP_GAIN", ccAmpGain, 0, 127, 64,
          "CHARACTER", "OUTPUT", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Decibels, "dB", {}, 3 },
        { "ampExpnt", "EG Amp Mod", "EG AMP MOD", "EG_AMP_MOD", ccEgAmpMod, 0, 127, 0,
          "CHARACTER", "OUTPUT", PRA32ControlKind::Toggle, false, 0,
          PRA32FormatKind::OnOff, "", enumOnOff(), 1 },

        // ---------------------------------------------------------------------
        // CHARACTER
        // ---------------------------------------------------------------------
        { "portaMode", "Voice Mode", "VOICE MODE", "VOICE_MODE", ccVoiceMode, 0, 127, 0,
          "CHARACTER", "VOICE", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumVoiceMode(), 2 },
        { "voiceAsgnMode", "Voice Assign Mode", "VOICE ASSIGN", "VOICE_ASGN_MODE", ccVoiceAsgnMode, 0, 127, 0,
          "CHARACTER", "VOICE", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumAsgnMode(), 1 },
        { "bthAmpMod", "Breath Amp Mod", "BREATH AMP", "BTH_AMP_MOD", ccBthAmpMod, 0, 127, 0,
          "CHARACTER", "VOICE", PRA32ControlKind::SteppedSelector, false, 0,
          PRA32FormatKind::Enum, "", enumBreathAmp(), 1 },
        { "egVelSens", "EG Velocity Sensitivity", "MOD VEL", "EG_VEL_SENS", ccEgVelSens, 0, 127, 0,
          "CHARACTER", "VOICE CHARACTER", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 1 },
        { "ampVelSens", "Amp Velocity Sensitivity", "AMP VEL", "AMP_VEL_SENS", ccAmpVelSens, 0, 127, 0,
          "CHARACTER", "VOICE CHARACTER", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 1 },
        { "aftTlfoAmt", "After Touch LFO Amount", "AT LFO AMT", "AFT_T_LFO_AMT", ccAftTlfoAmt, 0, 127, 0,
          "CHARACTER", "VOICE CHARACTER", PRA32ControlKind::Rotary, false, 0,
          PRA32FormatKind::Percent, "%", {}, 1 }
    };

    return parameters;
}

const SynthParamData* SynthParameters::find(const juce::String& id)
{
    for (const auto& p : getParameters())
        if (p.id == id)
            return &p;

    return nullptr;
}

#include "PluginProcessor.h"

// -----------------------------------------------------------------------------
// DUMMY HEADERS / WRAPPER DEFINITIONS FOR THE EMBEDDED ENGINE
// -----------------------------------------------------------------------------
#include "Arduino.h"
#include "EEPROM.h"
#include "I2S.h"

#ifndef PRA32_U2_USE_EMULATED_EEPROM
#define PRA32_U2_USE_EMULATED_EEPROM 1
#endif

// Provide dummy digitalWrite for JUCE environment
inline void digitalWrite(uint8_t pin, uint8_t val) {}

// Variables normally defined by the Arduino sketch / wrapper
EEPROMClass EEPROM;
I2SClass g_i2s_output;
uint8_t g_midi_ch = 0;

#include "pra32-u2-synth.h"

class PRA32Wrapper {
public:
    PRA32_U2_Synth<false, false, false, 4> synth;
};
// -----------------------------------------------------------------------------

//==============================================================================
static const char* factoryPresetsJson = R"(
{
  "_version       " : "PRA32-U2 Editor v2.12.0",
  "_comment       " : "Current  #0   #1   #2   #3   #4   #5   #6   #7     #8   #9   #10  #11  #12  #13  #14  #15  ",
  "OSC_1_WAVE     " : [ [0], [0  , 0  , 76 , 127, 0  , 25 , 0  , 0  , 76 , 0  , 25 , 0  , 0  , 25 , 127, 76 ] ],
  "MIXER_SUB_OSC  " : [ [64], [64 , 64 , 64 , 64 , 127, 96 , 127, 64 , 64 , 127, 64 , 64 , 64 , 64 , 1  , 64 ] ],
  "OSC_1_SHAPE    " : [ [64], [64 , 64 , 0  , 0  , 64 , 64 , 0  , 0  , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 100] ],
  "OSC_1_MORPH    " : [ [0], [0  , 127, 108, 64 , 0  , 127, 0  , 0  , 0  , 0  , 64 , 0  , 0  , 0  , 0  , 0  ] ],
  "OSC_2_WAVE     " : [ [0], [0  , 0  , 0  , 0  , 0  , 25 , 0  , 0  , 76 , 0  , 25 , 0  , 0  , 25 , 0  , 76 ] ],
  "MIXER_OSC_MIX  " : [ [64], [64 , 0  , 64 , 0  , 64 , 0  , 64 , 0  , 64 , 64 , 64 , 127, 64 , 64 , 0  , 64 ] ],
  "OSC_2_COARSE   " : [ [64], [64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 52 , 64 , 88 ] ],
  "OSC_2_PITCH    " : [ [72], [72 , 72 , 72 , 72 , 66 , 72 , 66 , 64 , 72 , 72 , 72 , 72 , 59 , 72 , 72 , 72 ] ],
  "FILTER_CUTOFF  " : [ [112], [112, 112, 88 , 127, 88 , 112, 40 , 127, 90 , 80 , 127, 60 , 40 , 30 , 100, 127] ],
  "FILTER_RESO    " : [ [48], [48 , 48 , 48 , 48 , 48 , 48 , 80 , 0  , 30 , 80 , 48 , 48 , 90 , 40 , 0  , 48 ] ],
  "FILTER_EG_AMT  " : [ [40], [40 , 64 , 64 , 64 , 76 , 64 , 88 , 64 , 64 , 100, 40 , 110, 100, 64 , 40 , 40 ] ],
  "FILTER_KEY_TRK " : [ [96], [96 , 96 , 96 , 96 , 64 , 64 , 64 , 64 , 96 , 96 , 96 , 96 , 96 , 96 , 96 , 96 ] ],
  "EG_ATTACK      " : [ [96], [96 , 32 , 32 , 32 , 32 , 32 , 32 , 0  , 110, 0  , 96 , 0  , 80 , 96 , 96 , 96 ] ],
  "EG_DECAY       " : [ [96], [96 , 32 , 96 , 32 , 32 , 96 , 100, 0  , 96 , 40 , 96 , 50 , 60 , 96 , 96 , 96 ] ],
  "EG_SUSTAIN     " : [ [0], [0  , 127, 0  , 127, 127, 0  , 0  , 127, 0  , 20 , 0  , 0  , 64 , 64 , 0  , 0  ] ],
  "EG_RELEASE     " : [ [32], [32 , 32 , 32 , 32 , 32 , 32 , 32 , 0  , 110, 32 , 32 , 32 , 32 , 32 , 32 , 32 ] ],
  "EG_OSC_AMT     " : [ [64], [64 , 64 , 72 , 64 , 64 , 72 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 ] ],
  "EG_OSC_DST     " : [ [0], [0  , 0  , 127, 0  , 0  , 127, 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "VOICE_MODE     " : [ [0], [0  , 0  , 0  , 0  , 127, 76 , 76 , 127, 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "PORTAMENTO     " : [ [48], [48 , 0  , 0  , 0  , 48 , 48 , 0  , 0  , 48 , 48 , 48 , 48 , 48 , 48 , 48 , 48 ] ],
  "LFO_WAVE       " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "LFO_FADE_TIME  " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "LFO_RATE       " : [ [80], [80 , 80 , 80 , 80 , 80 , 80 , 80 , 80 , 80 , 80 , 100, 80 , 80 , 80 , 80 , 80 ] ],
  "LFO_DEPTH      " : [ [0], [0  , 0  , 0  , 127, 8  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "LFO_OSC_AMT    " : [ [64], [64 , 64 , 64 , 64 , 96 , 72 , 64 , 64 , 64 , 64 , 90 , 64 , 64 , 64 , 64 , 64 ] ],
  "LFO_OSC_DST    " : [ [0], [0  , 0  , 127, 0  , 0  , 127, 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "LFO_FILTER_AMT " : [ [76], [76 , 76 , 76 , 64 , 64 , 64 , 76 , 64 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 76 ] ],
  "AMP_GAIN       " : [ [100], [100, 100, 120, 100, 100, 90 , 110, 100, 120, 120, 110, 127, 110, 127, 127, 120] ],
  "AMP_ATTACK     " : [ [32], [32 , 32 , 32 , 32 , 32 , 32 , 32 , 0  , 90 , 0  , 32 , 0  , 0  , 100, 0  , 0  ] ],
  "AMP_DECAY      " : [ [32], [32 , 32 , 32 , 32 , 32 , 32 , 32 , 0  , 32 , 40 , 32 , 50 , 32 , 32 , 30 , 80 ] ],
  "AMP_SUSTAIN    " : [ [127], [127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0  , 127, 127, 0  , 0  ] ],
  "AMP_RELEASE    " : [ [32], [32 , 32 , 32 , 32 , 32 , 32 , 32 , 0  , 90 , 32 , 32 , 32 , 40 , 100, 20 , 60 ] ],
  "FILTER_MODE    " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "P_BEND_RANGE   " : [ [2], [2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  , 2  ] ],
  "EG_AMP_MOD     " : [ [0], [0  , 127, 127, 127, 0  , 0  , 127, 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "REL_EQ_DECAY   " : [ [127], [127, 127, 127, 127, 127, 127, 127, 0  , 127, 127, 127, 127, 127, 127, 127, 127] ],
  "BTH_FILTER_AMT " : [ [64], [64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 ] ],
  "BTH_AMP_MOD    " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 64 , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "EG_VEL_SENS    " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "AMP_VEL_SENS   " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "AFT_T_LFO_AMT  " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "VOICE_ASGN_MODE" : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "PAN            " : [ [64], [64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 ] ],
  "OSC_DRIFT      " : [ [32], [32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 ] ],
  "OSC_SAW_W_MODE " : [ [127], [127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127] ],
  "CHORUS_MIX     " : [ [127], [127, 127, 127, 127, 127, 127, 127, 0  , 127, 0  , 0  , 127, 127, 127, 127, 127] ],
  "CHORUS_RATE    " : [ [64], [64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 ] ],
  "CHORUS_DEPTH   " : [ [64], [64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 ] ],
  "DELAY_LEVEL    " : [ [64], [64 , 64 , 64 , 64 , 64 , 64 , 64 , 0  , 80 , 0  , 0  , 50 , 64 , 64 , 64 , 80 ] ],
  "DELAY_MODE     " : [ [0], [0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ] ],
  "DELAY_TIME     " : [ [87], [87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 , 87 ] ],
  "DELAY_FEEDBACK " : [ [64], [64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 , 64 ] ],
  "_end           " : ""
}
)";

static juce::String getPresetKeyForParam(const juce::String& paramId, const juce::String& paramLabel)
{
    if (paramId == "osc1Wave") return "OSC_1_WAVE";
    if (paramId == "osc1Shape") return "OSC_1_SHAPE";
    if (paramId == "osc1Morph") return "OSC_1_MORPH";
    if (paramId == "osc2Wave") return "OSC_2_WAVE";
    if (paramId == "osc2Coarse") return "OSC_2_COARSE";
    if (paramId == "osc2Pitch") return "OSC_2_PITCH";
    if (paramId == "oscMix") return "MIXER_OSC_MIX";
    if (paramId == "subOsc") return "MIXER_SUB_OSC";
    if (paramId == "oscDrift") return "OSC_DRIFT";
    if (paramId == "sawWMode") return "OSC_SAW_W_MODE";
    if (paramId == "filterCutoff") return "FILTER_CUTOFF";
    if (paramId == "filterReso") return "FILTER_RESO";
    if (paramId == "filterMode") return "FILTER_MODE";
    if (paramId == "egFltAmt") return "FILTER_EG_AMT";
    if (paramId == "filterKeyTrk") return "FILTER_KEY_TRK";
    if (paramId == "bthFltAmt") return "BTH_FILTER_AMT";
    if (paramId == "relEqDcy") return "REL_EQ_DECAY";
    if (paramId == "egOscAmt") return "EG_OSC_AMT";
    if (paramId == "egAttack") return "EG_ATTACK";
    if (paramId == "egDecay") return "EG_DECAY";
    if (paramId == "egSustain") return "EG_SUSTAIN";
    if (paramId == "egRelease") return "EG_RELEASE";
    if (paramId == "ampAttack") return "AMP_ATTACK";
    if (paramId == "ampDecay") return "AMP_DECAY";
    if (paramId == "ampSustain") return "AMP_SUSTAIN";
    if (paramId == "ampRelease") return "AMP_RELEASE";
    if (paramId == "lfoWave") return "LFO_WAVE";
    if (paramId == "lfoRate") return "LFO_RATE";
    if (paramId == "lfoFltAmt") return "LFO_FILTER_AMT";
    if (paramId == "lfoOscAmt") return "LFO_OSC_AMT";
    if (paramId == "lfoFadeTime") return "LFO_FADE_TIME";
    if (paramId == "pbRange") return "P_BEND_RANGE";
    if (paramId == "choRate") return "CHORUS_RATE";
    if (paramId == "choDepth") return "CHORUS_DEPTH";
    if (paramId == "delayTime") return "DELAY_TIME";
    if (paramId == "delayDepth") return "DELAY_LEVEL";
    if (paramId == "pan") return "PAN";
    if (paramId == "ampGain") return "AMP_GAIN";
    if (paramId == "portaTime") return "PORTAMENTO";
    
    return paramLabel.replace(" ", "_");
}

//==============================================================================
PRA32ColorcoderAudioProcessor::PRA32ColorcoderAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", createParameterLayout()), synthWrapper(std::make_unique<PRA32Wrapper>())
{
    // Initialize the engine, just like in wasm_wrapper.cpp
    synthWrapper->synth.initialize();
    synthWrapper->synth.program_change(15); // Load Initial preset

    // Cache the atomic pointers for fast polling in the audio thread
    for (const auto& p : SynthParameters::getParameters()) {
        ParamBinding pb;
        pb.cc = p.cc;
        pb.valuePtr = apvts.getRawParameterValue(p.id);
        pb.lastValue = -1.0f; // Force update on first block
        paramBindings.push_back(pb);
    }
}

PRA32ColorcoderAudioProcessor::~PRA32ColorcoderAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PRA32ColorcoderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : SynthParameters::getParameters()) {
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{p.id, 1},
            p.label,
            p.min,
            p.max,
            p.def
        ));
    }
    return layout;
}

//==============================================================================
const juce::String PRA32ColorcoderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PRA32ColorcoderAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PRA32ColorcoderAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PRA32ColorcoderAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PRA32ColorcoderAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PRA32ColorcoderAudioProcessor::getNumPrograms()
{
    return 1;
}

int PRA32ColorcoderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PRA32ColorcoderAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PRA32ColorcoderAudioProcessor::getProgramName (int index)
{
    return {};
}

void PRA32ColorcoderAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void PRA32ColorcoderAudioProcessor::loadPreset(int index)
{
    juce::var parsedJson = juce::JSON::parse(factoryPresetsJson);
    if (!parsedJson.isObject()) return;
    
    auto* obj = parsedJson.getDynamicObject();
    
    for (const auto& p : SynthParameters::getParameters())
    {
        juce::String presetKey = getPresetKeyForParam(p.id, p.label);
        
        juce::var paramVar;
        bool found = false;
        for (auto& prop : obj->getProperties()) {
            if (prop.name.toString().trim() == presetKey) {
                paramVar = prop.value;
                found = true;
                break;
            }
        }
        
        if (found && paramVar.isArray()) {
            auto* arr = paramVar.getArray();
            if (arr->size() > 1 && arr->getReference(1).isArray()) {
                auto* presetArr = arr->getReference(1).getArray();
                if (index >= 0 && index < presetArr->size()) {
                    int newValue = presetArr->getReference(index);
                    if (auto* param = apvts.getParameter(p.id)) {
                        param->setValueNotifyingHost(param->convertTo0to1((float)newValue));
                    }
                }
            }
        }
    }
}

void PRA32ColorcoderAudioProcessor::loadPresetFromJson(const juce::String& jsonString)
{
    juce::var parsedJson = juce::JSON::parse(jsonString);
    if (!parsedJson.isObject()) return;
    
    auto* obj = parsedJson.getDynamicObject();
    
    for (const auto& p : SynthParameters::getParameters())
    {
        juce::String presetKey = getPresetKeyForParam(p.id, p.label);
        
        juce::var paramVar;
        bool found = false;
        for (auto& prop : obj->getProperties()) {
            if (prop.name.toString().trim() == presetKey) {
                paramVar = prop.value;
                found = true;
                break;
            }
        }
        
        if (found) {
            int newValue = p.def;
            if (paramVar.isArray()) {
                auto* arr = paramVar.getArray();
                if (arr->size() > 0) {
                    if (arr->getReference(0).isArray()) {
                        auto* currentArr = arr->getReference(0).getArray();
                        if (currentArr->size() > 0) {
                            newValue = currentArr->getReference(0);
                        }
                    } else {
                        newValue = arr->getReference(0);
                    }
                }
            } else {
                newValue = (int)paramVar;
            }
            
            if (auto* param = apvts.getParameter(p.id)) {
                param->setValueNotifyingHost(param->convertTo0to1((float)newValue));
            }
        }
    }
}

juce::String PRA32ColorcoderAudioProcessor::savePresetToJson()
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    
    for (const auto& p : SynthParameters::getParameters())
    {
        juce::String presetKey = getPresetKeyForParam(p.id, p.label);
        float currentVal = *apvts.getRawParameterValue(p.id);
        obj->setProperty(presetKey, (int)currentVal);
    }
    
    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
void PRA32ColorcoderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The PRA32 engine operates at a fixed 48kHz.
    // Calculate the phase increment for our linear interpolator resampler.
    phaseIncrement = 48000.0 / sampleRate;
    currentPhase = 1.0; // Force generating the first sample on first pass
    lastL = 0.0f;
    lastR = 0.0f;
    nextL = 0.0f;
    nextR = 0.0f;
}

void PRA32ColorcoderAudioProcessor::releaseResources()
{
}

bool PRA32ColorcoderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #endif

    return true;
  #endif
}

void PRA32ColorcoderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // 0. Update Synth Parameters from APVTS
    for (auto& pb : paramBindings) {
        if (pb.valuePtr != nullptr) {
            float val = pb.valuePtr->load(std::memory_order_relaxed);
            if (val != pb.lastValue) {
                pb.lastValue = val;
                synthWrapper->synth.control_change(pb.cc, static_cast<uint8_t>(val));
            }
        }
    }

    // 1. Process MIDI Events
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            synthWrapper->synth.note_on(msg.getNoteNumber(), msg.getVelocity());
        } else if (msg.isNoteOff()) {
            synthWrapper->synth.note_off(msg.getNoteNumber());
        } else if (msg.isController()) {
            synthWrapper->synth.control_change(msg.getControllerNumber(), msg.getControllerValue());
        } else if (msg.isPitchWheel()) {
            // JUCE pitch wheel is 0-16383, center 8192
            int value = msg.getPitchWheelValue();
            uint8_t lsb = value & 0x7F;
            uint8_t msb = (value >> 7) & 0x7F;
            synthWrapper->synth.pitch_bend(lsb, msb);
        } else if (msg.isProgramChange()) {
            synthWrapper->synth.program_change(msg.getProgramChangeNumber());
        }
    }

    // 2. Process Audio (with resampling from 48kHz)
    int numSamples = buffer.getNumSamples();
    float* channelDataL = buffer.getWritePointer(0);
    float* channelDataR = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // Fetch new samples from the 48kHz engine as needed
        while (currentPhase >= 1.0)
        {
            lastL = nextL;
            lastR = nextR;

            // Generate exactly one sample from the engine
            int16_t right_out = 0;
            int16_t left_out = synthWrapper->synth.process(0, 0, right_out);
            
            // Convert from int16 to float (-1.0 to 1.0)
            nextL = left_out / 32768.0f;
            nextR = right_out / 32768.0f;

            currentPhase -= 1.0;
        }

        // Linear interpolation
        float outL = lastL + (nextL - lastL) * currentPhase;
        float outR = lastR + (nextR - lastR) * currentPhase;

        channelDataL[i] = outL;
        if (channelDataR != nullptr)
            channelDataR[i] = outR;

        currentPhase += phaseIncrement;
    }
}

#include "PluginEditor.h"

//==============================================================================
bool PRA32ColorcoderAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PRA32ColorcoderAudioProcessor::createEditor()
{
    return new PRA32ColorcoderAudioProcessorEditor (*this);
}

//==============================================================================
void PRA32ColorcoderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PRA32ColorcoderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PRA32ColorcoderAudioProcessor();
}

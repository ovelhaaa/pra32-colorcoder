#include "PluginProcessor.h"

// -----------------------------------------------------------------------------
// Define the dummy globals required by the engine (mocking Arduino/embedded env)
// -----------------------------------------------------------------------------
EEPROMClass EEPROM;
I2SClass g_i2s_output;
uint8_t g_midi_ch = 0;

//==============================================================================
PRA32ColorcoderAudioProcessor::PRA32ColorcoderAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Initialize the engine, just like in wasm_wrapper.cpp
    synth.initialize();
    synth.program_change(15); // Load Initial preset

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
                synth.control_change(pb.cc, static_cast<uint8_t>(val));
            }
        }
    }

    // 1. Process MIDI Events
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            synth.note_on(message.getNoteNumber(), message.getVelocity());
        }
        else if (message.isNoteOff())
        {
            synth.note_off(message.getNoteNumber());
        }
        else if (message.isController())
        {
            synth.control_change(message.getControllerNumber(), message.getControllerValue());
        }
        else if (message.isPitchWheel())
        {
            // Pitch bend in JUCE is 0-16383, center is 8192
            // The synth expects lsb, msb.
            int pitchBendValue = message.getPitchWheelValue();
            uint8_t lsb = pitchBendValue & 0x7F;
            uint8_t msb = (pitchBendValue >> 7) & 0x7F;
            synth.pitch_bend(lsb, msb);
        }
        else if (message.isProgramChange())
        {
            synth.program_change(message.getProgramChangeNumber());
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

            int16_t right_out = 0;
            int16_t left_out = synth.process(0, 0, right_out);
            
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

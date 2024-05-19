/*
  ==============================================================================

    Copyright (c) 2022 - Gordon Webb

    This file is part of Rain4Unity.

    Wind4Unity1 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Wind4Unity1 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rain4Unity.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/
/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
//#include "PluginEditor.h"

//==============================================================================
Rain4UnityAudioProcessor::Rain4UnityAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    )
{
    //    Global Parameters
    addParameter(gain = new juce::AudioParameterFloat(
        "Master Gain", "Master Gain", 0.0f, 1.0f, 0.5f));
    //    Low Boiling
    addParameter(lbBPCutoff = new juce::AudioParameterFloat(
        "LBCutOff", "LB Cutoff", 15.f, 10000.f, 4000.0f));
    addParameter(lbBPQ = new juce::AudioParameterFloat(
        "LBQ", "LB Q Factor", 0.1f, 15.f, 2.6f));
    //  Wind Speed Parameters
    addParameter(dstAmplitude = new juce::AudioParameterFloat(
        "DstAmp", "Distant Gain", 0.0001f, 1.5f, 0.75f));
    addParameter(dstPan = new juce::AudioParameterFloat(
        "dstPan", "Distant Pan", 0.0f, 1.0f, 0.5f));
}

Rain4UnityAudioProcessor::~Rain4UnityAudioProcessor()
{
}

//==============================================================================
void Rain4UnityAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    //    Create DSP Spec
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    currentSpec = spec;

    //    Prepare DSP
    Prepare(spec);
}

void Rain4UnityAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Rain4UnityAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateSettings();
    dstProcess(buffer);

    buffer.applyGain(gain->get());
}

//==============================================================================
bool Rain4UnityAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Rain4UnityAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================

void Rain4UnityAudioProcessor::Prepare(const juce::dsp::ProcessSpec& spec)
{
    //    Prepare DST
    lbBPF.prepare(spec);
    lbBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    lbBPF.setCutoffFrequency(10.0f);
    lbBPF.setResonance(1.0f);
    lbBPF.reset();

    lbRngBPF.prepare(spec);
    lbRngBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    lbRngBPF.setCutoffFrequency(1000.0f);
    lbRngBPF.setResonance(60.0f);
    lbRngBPF.reset();

    lbLFO.initialise([](float x) { return std::sin(x); }, 128);

    whsBPF2.prepare(spec);
    whsBPF2.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    whsBPF2.setCutoffFrequency(1000.0f);
    whsBPF2.setResonance(60.0f);
    whsBPF2.reset();

    //    Prepare Howl
    howlBPF1.prepare(spec);
    howlBPF1.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    howlBPF1.setCutoffFrequency(400.0f);
    howlBPF1.setResonance(40.0f);
    howlBPF1.reset();

    howlBPF2.prepare(spec);
    howlBPF2.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    howlBPF2.setCutoffFrequency(200.0f);
    howlBPF2.setResonance(40.0f);
    howlBPF2.reset();

    howlOsc1.initialise([](float x) { return std::sin(x); }, 128);
    howlOsc2.initialise([](float x) { return std::sin(x); }, 128);

    howlBlockLPF1.prepare(0.5f, spec.maximumBlockSize, spec.sampleRate);
    howlBlockLPF2.prepare(0.4f, spec.maximumBlockSize, spec.sampleRate);
}

void Rain4UnityAudioProcessor::dstProcess(juce::AudioBuffer<float>& buffer)
{
    //    Get Buffer info
    int numSamples = buffer.getNumSamples();
    float FrameAmp = dstAmplitude->get();

    //    Distant Wind DSP Loop
    float pan[2];
    cosPan(pan, dstPan->get());

    for (int s = 0; s < numSamples; ++s)
    {
        float output = lbBPF.processSample(0, r.nextFloat() * 2.0f - 1.0f) * FrameAmp;
        buffer.addSample(0, s, output * pan[0]);
        buffer.addSample(1, s, output * pan[1]);
    }
  
    lbBPF.snapToZero();
}




void Rain4UnityAudioProcessor::updateSettings()
{
    //  UpdateWSCircularBuffer;

    float currentLBCutoff = lbBPCutoff->get();
    float currentDstResonance = lbBPQ->get();
    //gd.windSpeedCircularBuffer[gd.wSCBWriteIndex] = currentWindSpeed;
    //++gd.wSCBWriteIndex;
    //gd.wSCBWriteIndex = (gd.wSCBWriteIndex < wSCBSize) ? gd.wSCBWriteIndex : 0;

    

    // Update DST Filter Settings
    lbBPF.setCutoffFrequency(currentLBCutoff);
    lbBPF.setResonance(currentDstResonance);
}

void Rain4UnityAudioProcessor::cosPan(float* output, float pan)
{
    output[0] = juce::dsp::FastMathApproximations::cos((pan * 0.25f - 0.5f) * juce::MathConstants<float>::twoPi);
    output[1] = juce::dsp::FastMathApproximations::cos((pan * 0.25f - 0.25f) * juce::MathConstants<float>::twoPi);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Rain4UnityAudioProcessor();
}

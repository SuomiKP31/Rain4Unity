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
    //    Mid Boiling
    addParameter(mbBPCutoff = new juce::AudioParameterFloat(
        "MBCutOff", "MB Bandpass Cutoff", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 2341.0f));
    addParameter(mbBPQ = new juce::AudioParameterFloat(
        "MBQ", "MB Bandpass QFactor", 0.1f, 15.f, 2.4f));
    addParameter(mbRandomModulateAmplitude = new juce::AudioParameterFloat(
        "MBRM Amp", "MBRM Amp(dB)", 0.f, 24.f, 9.0f));
    addParameter(mbRngBPOscAmplitude = new juce::AudioParameterFloat(
        "MBRNGFreqBand", "MBRng BPF Freq Band", 100.f, 500.f, 435.0f));
    addParameter(mbRngBPCenterFrequency = new juce::AudioParameterFloat(
        "MBRNGCenterFreq", "MBRng BPF Center Freq", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 606.0f));
    addParameter(mbRngBPOscFrequency = new juce::AudioParameterFloat(
        "MBRNGOscFrequency", "MBRng Osc Frequency", 1.f, 100.f, 2.0f));
    addParameter(mbRngBPQ = new juce::AudioParameterFloat(
        "MBRng Q", "MBRng BP QFactor", 0.1f, 15.f, 9.0f));
    addParameter(dstAmplitude = new juce::AudioParameterFloat(
        "DstAmp", "Mid Boiling Gain", 0.0001f, 1.5f, 0.75f));

    // Low Boiling
    addParameter(lbRngBPOscAmplitude = new juce::AudioParameterFloat(
        "LBRNGFreqBand", "LBRng BPF Freq Band", 100.f, 500.f, 435.0f));
    addParameter(lbRngBPCenterFrequency = new juce::AudioParameterFloat(
        "LBRNGCenterFreq", "LBRng BPF Center Freq", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 606.0f));
    addParameter(lbRngBPOscFrequency = new juce::AudioParameterFloat(
        "LBRNGOscFrequency", "LBRng Osc Frequency", 1.f, 100.f, 2.0f));
    addParameter(lbRngBPQ = new juce::AudioParameterFloat(
        "LBRng Q", "LBRng BP QFactor", 0.1f, 15.f, 9.0f));
    addParameter(lbAmplitude = new juce::AudioParameterFloat(
        "lbAmp", "Low Boiling Gain", 0.0001f, 1.5f, 0.75f));
    addParameter(lbLPFCutoff = new juce::AudioParameterFloat(
        "LBLPFCutoff", "LBLPF Cutoff", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 3500.0f));
    addParameter(lbHPFCutoff = new juce::AudioParameterFloat(
        "LBHPFCutoff", "LBHPF Cutoff", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 400.0f));

    addParameter(dstPan = new juce::AudioParameterFloat(
        "dstPan", "Distant Pan", 0.0f, 1.0f, 0.5f));
}

static void mixAvg(juce::AudioBuffer<float>& buf, int destChannel, int destSample, float sample)
{
	auto buf_reader = buf.getReadPointer(destChannel);
    buf.setSample(destChannel, destSample, sample * 0.5f + buf_reader[destSample] * 0.5f);
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
    tempBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples()); // DO NOT RECREATE THIS BUFFER. Reuse it.

    updateSettings();

    midBoilProcess(buffer);
    lowBoilProcess(buffer);
    

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
    //    Mid-Boiling
    mbBPF.prepare(spec);
    mbBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    mbBPF.setCutoffFrequency(10.0f);
    mbBPF.setResonance(1.0f);
    mbBPF.reset();

    mbRngBPF.prepare(spec);
    mbRngBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    mbRngBPF.setCutoffFrequency(1000.0f);
    mbRngBPF.setResonance(60.0f);
    mbRngBPF.reset();

    mbRngBPOsc.initialise([](float x) { return std::sin(x); }, 128);


    //  Low-Boiling
    lbLPF.prepare(spec);
    lbLPF.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    lbLPF.setCutoffFrequency(400.f);
    lbLPF.setResonance(0.5f);
    lbLPF.reset();

    lbHPF.prepare(spec);
    lbHPF.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lbHPF.setCutoffFrequency(3500.0f);
    lbHPF.setResonance(1.2f);
    lbHPF.reset();
    // The L/HPF here are attenuation filters which get rid of some of the more out-of-place frequencies.
}

void Rain4UnityAudioProcessor::midBoilProcess(juce::AudioBuffer<float>& buffer)
{
    //    Get Buffer info
    int numSamples = buffer.getNumSamples();
    float FrameAmp = dstAmplitude->get();

    // Random Modulation - Volume following an LFO
    float randomModulationGain = mbRandomModulateAmplitude->get();
    randomModulationGain *= r.nextFloat() * 2.0f - 1.0f;
    randomModulationGain = juce::Decibels::decibelsToGain(randomModulationGain);

    // 2nd random BPF
    float centerFreq = mbRngBPCenterFrequency->get();
    float freqband = mbRngBPOscAmplitude->get();
    mbRngBPF.setCutoffFrequency(std::clamp(mbRngBPOsc.processSample(r.nextFloat() * 2.0f - 1.0f) * freqband + centerFreq, 25.f, 20000.0f));


    for (int s = 0; s < numSamples; ++s)
    {
        float output = mbBPF.processSample(0, r.nextFloat() * 2.0f - 1.0f) * FrameAmp;
        output = mbRngBPF.processSample(0, output);
        
        buffer.addSample(0, s, output); 
        buffer.addSample(1, s, output);
    }

    buffer.applyGain(randomModulationGain);
  
    mbBPF.snapToZero();
    mbRngBPF.snapToZero();
}

void Rain4UnityAudioProcessor::lowBoilProcess(juce::AudioBuffer<float>& buffer)
{
    tempBuffer.clear();

    int numSamples = buffer.getNumSamples();

    for (int s = 0; s < numSamples; ++s)
    {
        float output = pr.nextFloat();

        tempBuffer.addSample(0, s, output);
        tempBuffer.addSample(1, s, output);
    }

    tempBuffer.applyGain(0.2f);

    buffer.addFrom(0, 0, tempBuffer, 0, 0, numSamples, 1);
    buffer.addFrom(1, 0, tempBuffer, 1, 0, numSamples, 1);

    for (int s = 0; s < numSamples; ++s)
    {
        float output = lbLPF.processSample(0, buffer.getSample(0, s));
        output = lbHPF.processSample(0, output);
        buffer.setSample(0, s, output);

        output = lbLPF.processSample(1, buffer.getSample(1, s));
        output = lbHPF.processSample(1, output);
        buffer.setSample(1, s, output);
    }


}

void Rain4UnityAudioProcessor::updateSettings()
{
    //  UpdateWSCircularBuffer;

    float currentMBCutoff = mbBPCutoff->get();
    float currentDstResonance = mbBPQ->get();
    float currentMBRngFrequency = mbRngBPOscFrequency->get();
    float currentMBRngQ = mbRngBPQ->get();

    float currentLBLPFCutoff = lbLPFCutoff->get();
    float currentLBHPFCutoff = lbHPFCutoff->get();
    float currentLBRngFrequency = lbRngBPOscFrequency->get();

    // Update DST Filter Settings
    mbBPF.setCutoffFrequency(currentMBCutoff);
    mbBPF.setResonance(currentDstResonance);
    mbRngBPF.setResonance(currentMBRngQ);
    mbRngBPOsc.setFrequency(currentMBRngFrequency);

    lbLPF.setCutoffFrequency(currentLBLPFCutoff);
    lbHPF.setCutoffFrequency(currentLBHPFCutoff);
    lbRngBPOsc.setFrequency(currentLBRngFrequency);


    
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

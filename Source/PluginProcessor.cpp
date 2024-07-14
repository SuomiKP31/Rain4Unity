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
    addParameter(mbGain = new juce::AudioParameterFloat(
        "MB Gain", "Mid Boiling Gain", 0.0001f, 1.0f, 0.01f));
    addParameter(lbGain = new juce::AudioParameterFloat(
        "LB Gain", "Low Boiling Gain", 0.0001f, 1.0f, 0.03f));
    addParameter(stGain = new juce::AudioParameterFloat(
        "Stereo Gain", "Stereo Gain", 0.0001f, 1.0f, 0.15f));
    addParameter(dropGain = new juce::AudioParameterFloat(
        "Drop Gain", "Drop Gain", 0.0001f, 1.0f, 0.6f));

    //    Mid Boiling
    addParameter(mbBPCutoff = new juce::AudioParameterFloat(
        "MBCutOff", "MB Bandpass Cutoff", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 500.0f));
    addParameter(mbBPQ = new juce::AudioParameterFloat(
        "MBQ", "MB Bandpass QFactor", 0.1f, 15.f, 2.75f));
    addParameter(mbRandomModulateAmplitude = new juce::AudioParameterFloat(
        "MBRM Amp", "MBRM Amp(dB)", 0.f, 9.f, 7.0f));
    addParameter(mbRngBPOscAmplitude = new juce::AudioParameterFloat(
        "MBRNGFreqBand", "MBRng BPF Freq Band", 100.f, 500.f, 365.0f));
    addParameter(mbRngBPCenterFrequency = new juce::AudioParameterFloat(
        "MBRNGCenterFreq", "MBRng BPF Center Freq", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 940.0f));
    addParameter(mbRngBPOscFrequency = new juce::AudioParameterFloat(
        "MBRNGOscFrequency", "MBRng Osc Frequency", 1.f, 100.f, 4.0f));
    addParameter(mbRngBPQ = new juce::AudioParameterFloat(
        "MBRng Q", "MBRng BP QFactor", 0.1f, 15.f, 7.5f));


    // Low Boiling
    addParameter(lbRngBPOscAmplitude = new juce::AudioParameterFloat(
        "LBRNGFreqBand", "LBRng BPF Freq Band", 100.f, 500.f, 230.0f));
    addParameter(lbRngBPCenterFrequency = new juce::AudioParameterFloat(
        "LBRNGCenterFreq", "LBRng BPF Center Freq", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 1200.0f));
    addParameter(lbRngBPOscFrequency = new juce::AudioParameterFloat(
        "LBRNGOscFrequency", "LBRng Osc Frequency", 1.f, 100.f, 30.0f));
    addParameter(lbRngBPQ = new juce::AudioParameterFloat(
        "LBRng Q", "LBRng BP QFactor", 0.1f, 15.f, 1.4f));
    addParameter(lbLPFCutoff = new juce::AudioParameterFloat(
        "LBLPFCutoff", "LBLPF Cutoff", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 3500.0f));
    addParameter(lbHPFCutoff = new juce::AudioParameterFloat(
        "LBHPFCutoff", "LBHPF Cutoff", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 750.0f));

    // Stereo
    addParameter(stLPFCutoff = new juce::AudioParameterFloat(
        "Stereo LPF", "StereoLPF", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 1675.0f));
    addParameter(stHPFCutoff = new juce::AudioParameterFloat(
        "Stereo HPF", "StereoHPF", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 875.0f));
    addParameter(stPeakFreq = new juce::AudioParameterFloat(
        "Stereo Peak", "Stereo Peak", juce::NormalisableRange<float>(15.f, 10000.f, 1, 0.25), 1000.0f));

    // Drop
    addParameter(dropRetriggerTime = new juce::AudioParameterFloat(
        "Drop Length", "Drop ReTrigger Time", 0.1f, 0.3f, 0.15f));
    addParameter(dropFreqInterval = new juce::AudioParameterFloat(
        "Drop Freq Interval", "Drop Freq Coef", 0.f, 9.0f, 3.0f));
    addParameter(dropTimeInterval = new juce::AudioParameterFloat(
        "Drop Time Interval", "Drop Time Coef", 0.f, 9.0f, 6.0f));
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
    stereoBoilProcess(buffer);
    dropProcess(buffer);

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
    mbRngBPF.setResonance(10.0f);
    mbRngBPF.reset();

    mbRngBPOsc.initialise([](float x) { return std::sin(x); }, 128);


    //  Low-Boiling
    lbLPF.prepare(spec);
    lbLPF.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    lbLPF.setCutoffFrequency(800.f);
    lbLPF.setResonance(0.5f);
    lbLPF.reset();

    lbHPF.prepare(spec);
    lbHPF.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lbHPF.setCutoffFrequency(3500.0f);
    lbHPF.setResonance(1.2f);
    lbHPF.reset();

    lbRngBPF.prepare(spec);
    lbRngBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    lbRngBPF.setCutoffFrequency(1000.f);
    lbRngBPF.setResonance(15.0f);
    lbRngBPF.reset();

    lbRngBPOsc.initialise([](float x) { return std::sin(x); }, 128);
    // The L/HPF here are attenuation filters which get rid of some of the more out-of-place frequencies.

    // Stereo
    stLPeakF.prepare(spec);
    stLPeakF.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(spec.sampleRate, 1000, 1.0f, 1);
    stLPeakF.reset();

    stRPeakF.prepare(spec);
    stRPeakF.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(spec.sampleRate, 1000, 1.0f, 1);
    stRPeakF.reset();

    stLPF.prepare(spec);
    stLPF.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    stLPF.setCutoffFrequency(800.f);
    stLPF.setResonance(0.8f);
    stLPF.reset();

    stHPF.prepare(spec);
    stHPF.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    stHPF.setCutoffFrequency(2750.f);
    stHPF.setResonance(0.8f);
    stHPF.reset();

    // Drop
    dropWave.set_spec(spec);
}

void Rain4UnityAudioProcessor::midBoilProcess(juce::AudioBuffer<float>& buffer)
{
    //    Get Buffer info
    int numSamples = buffer.getNumSamples();
    float FrameAmp = mbGain->get();

    // Random Modulation - Volume following an LFO
    float randomModulationGain = mbRandomModulateAmplitude->get();
    randomModulationGain *= r.nextFloat() * 2.0f - 1.0f;
    randomModulationGain = juce::Decibels::decibelsToGain(randomModulationGain);

    // random BPF
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

    // random BPF
    float centerFreq = lbRngBPCenterFrequency->get();
    float freqband = lbRngBPOscAmplitude->get();
    lbRngBPF.setCutoffFrequency(std::clamp(lbRngBPOsc.processSample(r.nextFloat() * 2.0f - 1.0f) * freqband + centerFreq, 25.f, 20000.0f));

    for (int s = 0; s < numSamples; ++s)
    {
        float output = pr.nextFloat() * 2.0f - 1.0f;
        output = lbRngBPF.processSample(0, output);
        tempBuffer.addSample(0, s, output);
        tempBuffer.addSample(1, s, output);
    }

    tempBuffer.applyGain(lbGain->get());

    buffer.addFrom(0, 0, tempBuffer, 0, 0, numSamples, 1);
    buffer.addFrom(1, 0, tempBuffer, 1, 0, numSamples, 1);

    // Filter the two boiling parts here
    for (int s = 0; s < numSamples; ++s)
    {
        float output = lbLPF.processSample(0, buffer.getSample(0, s));
        output = lbHPF.processSample(0, output);
        buffer.setSample(0, s, output);

        output = lbLPF.processSample(1, buffer.getSample(1, s));
        output = lbHPF.processSample(1, output);
        buffer.setSample(1, s, output);
    }

    lbLPF.snapToZero();
    lbHPF.snapToZero();
    lbRngBPF.snapToZero();
}

void Rain4UnityAudioProcessor::stereoBoilProcess(juce::AudioBuffer<float>& buffer)
{
    tempBuffer.clear();
    float currentSTGain = stGain->get() / 2.0f;
    int numSamples = buffer.getNumSamples();

    float panL1[2], panR1[2];
    float outputL1,outputR1;

    // Hard Pan
    cosPan(panL1, 0.f);
    cosPan(panR1, 1.0f);

    // Stereo Pink noises & Filtering (2 Layers)
    for (int s = 0; s < numSamples; ++s)
    {
        outputL1 = stereoPnL1.nextFloat();
        stProcessSample(0, outputL1);
        outputR1 = stereoPnR1.nextFloat();
        stProcessSample(1, outputR1);
        tempBuffer.setSample(0, s, outputL1 * panL1[0] + outputR1 * panR1[0]);
        tempBuffer.setSample(1, s, outputL1 * panL1[1] + outputR1 * panR1[1]);
    }

    tempBuffer.applyGain(currentSTGain);
    buffer.addFrom(0, 0, tempBuffer, 0, 0, numSamples, 1);
    buffer.addFrom(1, 0, tempBuffer, 1, 0, numSamples, 1);
    
    stLPF.snapToZero();
    stHPF.snapToZero();
    stLPeakF.snapToZero();
    stRPeakF.snapToZero();
}

void Rain4UnityAudioProcessor::dropProcess(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
	// Trigger
    if(dropWave.finished() && r.nextFloat() > 0.9f)
    {
        dropWave.reset(dropRetriggerTime->get(), dropTimeInterval->get(), dropFreqInterval->get());
    }
    // Process
    tempBuffer.clear();
    // a. Pan
    float pan[2];
    cosPan(pan, dropWave.pan);
    // b. Generate Sample from Wave
    float output;
    for (int s = 0; s < numSamples; ++s)
    {
        output = dropWave.GetNext();
        tempBuffer.setSample(0, s, output * pan[0]);
        tempBuffer.setSample(1, s, output * pan[1]);
    }

    tempBuffer.applyGain(dropGain->get());

    buffer.addFrom(0, 0, tempBuffer, 0, 0, numSamples, 1);
    buffer.addFrom(1, 0, tempBuffer, 1, 0, numSamples, 1);

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
    float currentLBBPQ = lbRngBPQ->get();

    float currentSTLPFCutoff = stLPFCutoff->get();
    float currentSTHPFCutoff = stHPFCutoff->get();
    float currentSTPeakFreq = stPeakFreq->get();

    // Update DST Filter Settings
    mbBPF.setCutoffFrequency(currentMBCutoff);
    mbBPF.setResonance(currentDstResonance);
    mbRngBPF.setResonance(currentMBRngQ);
    mbRngBPOsc.setFrequency(currentMBRngFrequency);

    lbLPF.setCutoffFrequency(currentLBLPFCutoff);
    lbHPF.setCutoffFrequency(currentLBHPFCutoff);
    lbRngBPOsc.setFrequency(currentLBRngFrequency);
    lbRngBPF.setResonance(currentLBBPQ);

    stLPF.setCutoffFrequency(currentSTLPFCutoff);
    stHPF.setCutoffFrequency(currentSTHPFCutoff);
    stLPeakF.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), currentSTPeakFreq, 1, 1.25f);
    stRPeakF.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), currentSTPeakFreq, 1, 1.25f);
    
}

void Rain4UnityAudioProcessor::cosPan(float* output, float pan)
{
    output[0] = juce::dsp::FastMathApproximations::cos((pan * 0.25f - 0.5f) * juce::MathConstants<float>::twoPi);
    output[1] = juce::dsp::FastMathApproximations::cos((pan * 0.25f - 0.25f) * juce::MathConstants<float>::twoPi);
}

void Rain4UnityAudioProcessor::stProcessSample(int channel, float& sample)
{
    sample = channel == 0 ? stLPeakF.processSample(sample) : stRPeakF.processSample(sample);
    sample = stLPF.processSample(channel, sample);
    sample = stHPF.processSample(channel, sample);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Rain4UnityAudioProcessor();
}

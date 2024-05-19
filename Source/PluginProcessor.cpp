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
    //  Wind Speed Parameters
    addParameter(dstAmplitude = new juce::AudioParameterFloat(
        "DstAmp", "Distant Gain", 0.0001f, 1.5f, 0.75f));
    addParameter(windSpeed = new juce::AudioParameterFloat(
        "windSpeed", "Wind Speed", 0.01f, 40.0f, 1.0f));
    addParameter(dstIntensity = new juce::AudioParameterFloat(
        "intensity", "dst Intensity", 1.0f, 50.0f, 30.0f));
    addParameter(dstResonance = new juce::AudioParameterFloat(
        "dstResonance", "dst Resonance", 0.1f, 50.0f, 1.0f));
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
    dstBPF.prepare(spec);
    dstBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    dstBPF.setCutoffFrequency(10.0f);
    dstBPF.setResonance(1.0f);
    dstBPF.reset();

    //    Prepare Whistle
    whsBPF1.prepare(spec);
    whsBPF1.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    whsBPF1.setCutoffFrequency(1000.0f);
    whsBPF1.setResonance(60.0f);
    whsBPF1.reset();

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
        float output = dstBPF.processSample(0, r.nextFloat() * 2.0f - 1.0f) * FrameAmp;
        buffer.addSample(0, s, output * pan[0]);
        buffer.addSample(1, s, output * pan[1]);
    }
  
    dstBPF.snapToZero();
}




void Rain4UnityAudioProcessor::updateSettings()
{
    //  UpdateWSCircularBuffer;

    float currentWindSpeed = windSpeed->get();
    gd.windSpeedCircularBuffer[gd.wSCBWriteIndex] = currentWindSpeed;
    ++gd.wSCBWriteIndex;
    gd.wSCBWriteIndex = (gd.wSCBWriteIndex < wSCBSize) ? gd.wSCBWriteIndex : 0;


    // Update DST
    float currentDstIntensity = dstIntensity->get();
    float currentDstResonance = dstResonance->get();

    // Update DST Filter Settings
    dstBPF.setCutoffFrequency(currentWindSpeed * currentDstIntensity);
    dstBPF.setResonance(currentDstResonance);

    /*// Update Whistle
    
    wd.whsWSCBReadIndex1 = gd.wSCBWriteIndex - (int)(pd.whistlePan1 * maxPanFrames);
    wd.whsWSCBReadIndex1 = (wd.whsWSCBReadIndex1 < 0) ? wd.whsWSCBReadIndex1 + wSCBSize : wd.whsWSCBReadIndex1;
    wd.whsWSCBReadIndex2 = gd.wSCBWriteIndex - (int)(pd.whistlePan2 * maxPanFrames);
    wd.whsWSCBReadIndex2 = (wd.whsWSCBReadIndex2 < 0) ? wd.whsWSCBReadIndex2 + wSCBSize : wd.whsWSCBReadIndex2;
    wd.whsWindSpeed1 = gd.windSpeedCircularBuffer[wd.whsWSCBReadIndex1];
    wd.whsWindSpeed2 = gd.windSpeedCircularBuffer[wd.whsWSCBReadIndex2];
    whsBPF1.setCutoffFrequency(wd.whsWindSpeed1 * 8.0f + 600.0f);
    whsBPF2.setCutoffFrequency(wd.whsWindSpeed2 * 20.0f + 1000.0f);

    //    Update Howl
    hd.howlWSCBReadIndex1 = gd.wSCBWriteIndex - (int)(pd.howlPan1 * maxPanFrames);
    hd.howlWSCBReadIndex1 = (hd.howlWSCBReadIndex1 < 0) ? hd.howlWSCBReadIndex1 + wSCBSize : hd.howlWSCBReadIndex1;
    hd.howlWSCBReadIndex2 = gd.wSCBWriteIndex - (int)(pd.howlPan2 * maxPanFrames);
    hd.howlWSCBReadIndex2 = (hd.howlWSCBReadIndex2 < 0) ? hd.howlWSCBReadIndex2 + wSCBSize : hd.howlWSCBReadIndex2;
    hd.howlWindSpeed1 = gd.windSpeedCircularBuffer[hd.howlWSCBReadIndex1];
    hd.howlWindSpeed2 = gd.windSpeedCircularBuffer[hd.howlWSCBReadIndex2];*/
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

/*
  ==============================================================================

    RainDropWave.h
    Created: 25 May 2024 3:01:15pm
    Author:  KP31

  ==============================================================================
*/

#pragma once
#include <cmath>
#include <JuceHeader.h>

class RainDropWave
{

private:
    float time = 0.f;
    float t_init = 0.001f;
    float delta_t_1 = 0.002f;
    float delta_t_2 = 0.006f;
    float delta_t_3 = 0.012f;
    float A0 = 1.0f;
    float A1 = 20.0f;
    float k = 3.0f;
    float m = 6.f;
    float f = 50.0f;

    juce::dsp::ProcessSpec currentSpec;
    juce::Random r;

public:
    float pan = 0.5;
    void set_spec(juce::dsp::ProcessSpec spec)
    {
        currentSpec = spec;
    }

    // Called for each trigger
    void reset(float end_time, float interval_coeff, float freq_coeff);
    bool finished() const;

    // Called for each sample
    float GetNext();
    float running_max_normalize(float value);
    float rand_num_new();
    float rand_num_new(float endtime);

    // valid when -1 <= x <= 1
    static float fast_acos(float x)
    {
        if (x < -1)
            return 0.f;
        if (x > 1)
            return 0.f;

        float negate = float(x < 0);
        x = abs(x);
        float ret = -0.0187293;
        ret = ret * x;
        ret = ret + 0.0742610;
        ret = ret * x;
        ret = ret - 0.2121144;
        ret = ret * x;
        ret = ret + 1.5707288;
        ret = ret * sqrt(1.0 - x);
        ret = ret - 2 * negate * ret;
        return negate * juce::MathConstants<float>::pi + ret;
    }

	static float remap(float value, float from1, float to1, float from2, float to2) {
        return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
    }
};

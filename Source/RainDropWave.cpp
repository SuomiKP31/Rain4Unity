/*
  ==============================================================================

    RainDropWave.cpp
    Created: 25 May 2024 3:01:15pm
    Author:  KP31

  ==============================================================================
*/

#include "RainDropWave.h"

void RainDropWave::reset(float end_time, float interval_coeff, float freq_coeff)
{
    t_init = rand_num_new(end_time);
    delta_t_1 = interval_coeff * rand_num_new(0.002f);
    delta_t_2 = 0.002f + interval_coeff * rand_num_new(0.004f);
    delta_t_3 = 0.006f + interval_coeff * rand_num_new(0.006f);
    A0 = 1.0f;
    A1 = 1.2f;
    k = 3.0f;
    m = 3 + freq_coeff * rand_num_new(12);
    f = 1000 + freq_coeff * rand_num_new(1000);
    time = 0.f;
    pan = remap(r.nextFloat(), 0, 0.48f, 1.f, 0.52f);
}

bool RainDropWave::finished() const
{
    return time > t_init + delta_t_3;
}

float RainDropWave::GetNext()
{
    float value = 0.f;

    float unit_time = static_cast<float>(1.0 / currentSpec.sampleRate);
    time += unit_time;

    if (time < t_init)
    {
        return value;
    }
    if (time < (t_init + delta_t_1))
    {
        // t range from -1 to 1
        //float t = 2 * (time - t_init) / delta_t_1 - 1.f;
        return sin(time);
    }

    if (time < (t_init + delta_t_2))
    {
        return value;
    }
    if (time < (t_init + delta_t_3))
    {
        value = exp(-m * (time - t_init - delta_t_2) / (delta_t_3 - delta_t_2)) * A1 * sin(juce::MathConstants<float>::twoPi * f * (time - t_init - delta_t_2));
        return running_max_normalize(value);
    }
    return value; // return 0;
}

float RainDropWave::running_max_normalize(float value)
{
    return value / juce::MathConstants<float>::pi;
}

float RainDropWave::rand_num_new()
{
    return r.nextFloat();
}

float RainDropWave::rand_num_new(float endtime)
{
    return r.nextFloat() * endtime;
}

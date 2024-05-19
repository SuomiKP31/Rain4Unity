#include <JuceHeader.h>

#pragma once
class PinkNoise
{
protected:
	float b0, b1, b2, b3, b4, b5, b6;
	juce::Random r;
public:
	float nextFloat();
};


# Real-time Rain synthesizer
This is a C++ plugin/standalone program that makes rain sound in real time.
It is tested as Unity Plugin (albeit some parameter interface does not work properly) and a standalone program.
It should theorectically work as an VST3, if you compile it that way.

# To Compile
If you want to compile it, follow the instruction of another plugin I made (JUCE VST3 Mixer)[https://github.com/SuomiKP31/JUCE_VST3_Mixer] and install juce.
Use Projucer to load the .jucer file.

It was tested in VS2022.

# Reference

Mostly I composed the noises following the components from this (blog)[https://blog.audiokinetic.com/fr/generating-rain-with-pure-synthesis/].
The raindrop part is an improved version of Shaokang Li's (raindrop generator)[raindrop generator].
Overall, I like it. I listen to it when I sleep.

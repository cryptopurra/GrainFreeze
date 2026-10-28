#pragma once
#include <JuceHeader.h>
#include <vector>

// Factory presets: original sound-design starting points.
// `values` are real (un-normalised) parameter values keyed by parameter ID.
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
    struct Env { int index; std::vector<juce::Point<float>> pts; };
    std::vector<Env> envs;          // optional custom envelope curves
    juce::String seqPattern;        // optional, '1' = live, '2' = frozen; empty = default pattern
};

const std::vector<FactoryPreset>& factoryPresets();
juce::StringArray factoryPresetNames();

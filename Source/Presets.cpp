#include "Presets.h"
#include "Parameters.h"

using Pt = juce::Point<float>;

static std::vector<Pt> decay (float power)
{
    std::vector<Pt> v;
    for (float x : { 0.0f, 0.08f, 0.18f, 0.3f, 0.45f, 0.62f, 0.8f, 1.0f })
        v.push_back ({ x, std::pow (1.0f - x, power) });
    return v;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets =
    {
        { "Default Preset", {}, {}, "" },

        { "Frozen Pad",
          { { pid::grainSize, 3000.f }, { pid::grainOverlap, 0.9f }, { pid::scanSpeed, 0.08f },
            { pid::specSmear, 0.55f }, { pid::lowpass, 0.8f } },
          {}, "" },

        { "Metal Fade",
          { { pid::grainSize, 600.f }, { pid::grainOverlap, 0.7f }, { pid::robotize, 0.6f },
            { "env0_enable", 1.f }, { pid::specSmear, 0.2f } },
          { { 0, decay (1.4f) } }, "" },

        { "Crazy Drop",
          { { "env1_enable", 1.f }, { "env1_dp_pit", 1.f }, { pid::grainSize, 1400.f },
            { pid::grainOverlap, 0.6f }, { "env0_enable", 1.f } },
          { { 1, { { 0.f, 0.5f }, { 0.35f, 0.6f }, { 1.f, 0.f } } }, { 0, decay (2.8f) } }, "" },

        { "Spectral Smear",
          { { pid::specSmear, 0.85f }, { pid::specMix, 1.f }, { pid::grainSize, 2500.f },
            { pid::grainOverlap, 0.8f }, { pid::scanSpeed, 0.3f } },
          {}, "" },

        { "Tape Stop",
          { { pid::grainSize, 1800.f }, { pid::grainOverlap, 0.4f }, { "env0_enable", 1.f } },
          { { 0, { { 0.f, 1.f }, { 0.55f, 0.85f }, { 1.f, 0.f } } } }, "" },

        { "Octave Cloud",
          { { pid::grainPitch, 50.f }, { pid::grainSize, 1500.f }, { pid::grainOverlap, 0.85f },
            { pid::scanSpeed, 0.2f }, { pid::specSmear, 0.3f } },
          {}, "" },

        { "Stutter Gate",
          { { pid::seqOn, 1.f }, { pid::seqLength, 16.f }, { pid::seqResolution, 3.f },
            { pid::grainSize, 700.f }, { pid::grainOverlap, 0.3f } },
          {}, "2211221122112121" },

        { "Lo-Fi Hold",
          { { pid::specHold, 0.7f }, { pid::reducer, 0.35f }, { pid::lowpass, 0.55f },
            { pid::grainSize, 2200.f }, { pid::grainOverlap, 0.75f } },
          {}, "" },
    };
    return presets;
}

juce::StringArray factoryPresetNames()
{
    juce::StringArray n;
    for (auto& p : factoryPresets()) n.add (p.name);
    return n;
}

#include "Parameters.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace juce;

const char* modDestShort (ModDest d)
{
    switch (d)
    {
        case ModDest::speed:     return "spd";
        case ModDest::pitch:     return "pit";
        case ModDest::volume:    return "vol";
        case ModDest::overlap:   return "ovl";
        case ModDest::grainSize: return "grs";
        case ModDest::specMix:   return "spm";
        case ModDest::specHold:  return "sph";
        case ModDest::highpass:  return "hp";
        case ModDest::lowpass:   return "lp";
        case ModDest::robotize:  return "rob";
        default:                 return "non";
    }
}

const char* modDestLabel (ModDest d)
{
    switch (d)
    {
        case ModDest::speed:     return "Speed";
        case ModDest::pitch:     return "Pitch";
        case ModDest::volume:    return "Volume";
        case ModDest::overlap:   return "Overlap";
        case ModDest::grainSize: return "Grain Size";
        case ModDest::specMix:   return "Spec. Mix";
        case ModDest::specHold:  return "Spec. Hld";
        case ModDest::highpass:  return "Highpass";
        case ModDest::lowpass:   return "Lowpass";
        case ModDest::robotize:  return "Robotize";
        default:                 return "-";
    }
}

const std::vector<EnvSpec>& envSpecs()
{
    // Matches TimeFreeze's tab destinations (verified from the UI).
    static const std::vector<EnvSpec> specs =
    {
        { "Time",   { ModDest::speed } },
        { "Pitch",  { ModDest::pitch } },
        { "Misc 1", { ModDest::volume, ModDest::overlap, ModDest::grainSize, ModDest::specMix, ModDest::specHold } },
        { "Misc 2", { ModDest::volume, ModDest::overlap, ModDest::grainSize, ModDest::specMix, ModDest::specHold } },
        { "Misc 3", { ModDest::volume, ModDest::highpass, ModDest::lowpass, ModDest::specMix, ModDest::robotize } },
        { "Misc 4", { ModDest::volume, ModDest::highpass, ModDest::lowpass, ModDest::specMix, ModDest::robotize } },
    };
    return specs;
}

StringArray resolutionNames() { return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
StringArray barsNames()       { return { "1/2", "1", "2", "4", "8", "16" }; }

static NormalisableRange<float> pct() { return { 0.0f, 1.0f, 0.0001f }; }

APVTS::ParameterLayout createParameterLayout()
{
    APVTS::ParameterLayout layout;

    // --- global / source ---
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::freeze, 1 },     "Freeze", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::reset, 1 },      "Reset",  false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::bypass, 1 },     "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::limiterOn, 1 },  "Limiter", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::inputGain, 1 },  "Input Gain",  NormalisableRange<float> (-48.f, 6.f, 0.01f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::outputGain, 1 }, "Output Gain", NormalisableRange<float> (-48.f, 6.f, 0.01f), 0.f));

    // --- granular core ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainSize, 1 },    "Grain Size",    NormalisableRange<float> (128.f, 4096.f, 1.f, 0.5f), 2048.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainOverlap, 1 }, "Overlap",       pct(), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainPitch, 1 },   "Pitch",         NormalisableRange<float> (-100.f, 100.f, 0.1f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::scanSpeed, 1 },    "Speed",         pct(), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::volume, 1 },       "Volume",        NormalisableRange<float> (0.f, 2.f, 0.001f), 1.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::reducer, 1 },      "Reducer",       pct(), 1.0f));

    // --- spectral ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::specMix, 1 },   "Spectral Mix",   pct(), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::specSmear, 1 }, "Spectral Smear", pct(), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::specHold, 1 },  "Spectral Hold",  pct(), 0.0f));

    // --- tone ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::highpass, 1 }, "Highpass", pct(), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::lowpass, 1 },  "Lowpass",  pct(), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::robotize, 1 }, "Robotize", pct(), 0.0f));

    // --- step sequencer ---
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { pid::seqOn, 1 },        "Sequencer On", false));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { pid::seqLength, 1 },    "Seq Length", 1, 32, 16));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::seqResolution, 1 },"Seq Resolution", resolutionNames(), 3));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { pid::seqOffset, 1 },    "Seq Offset", NormalisableRange<float> (-0.5f, 0.5f, 0.001f), 0.f));

    // --- voice section ---
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::voiceEnable, 1 },  "Voice Source", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::voiceGain, 1 },    "Voice Gain", NormalisableRange<float> (-48.f, 6.f, 0.01f), 0.f));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::stretchSync, 1 },  "Stretch Sync", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::stretchRatio, 1 }, "Stretch", NormalisableRange<float> (0.25f, 4.f, 0.001f, 0.5f), 1.f));

    // --- envelopes ---
    const auto& specs = envSpecs();
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        const auto& spec = specs[(size_t) n];
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { pid::envField (n, "enable"), 1 }, spec.name + " Enable", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::envField (n, "mode"), 1 },   spec.name + " Mode", StringArray { "Normal", "Step" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::envField (n, "loop"), 1 },   spec.name + " Loop", StringArray { "One-Shot", "Loop", "Ping-Pong" }, 1));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::envField (n, "bars"), 1 },   spec.name + " Bars", barsNames(), 2));
        layout.add (std::make_unique<AudioParameterInt>    (ParameterID { pid::envField (n, "steps"), 1 },  spec.name + " Steps", 2, 32, 16));

        for (auto d : spec.dests)
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { pid::envDepth (n, d), 1 },
                spec.name + " " + modDestLabel (d),
                NormalisableRange<float> (-1.f, 1.f, 0.001f), 0.f));
    }

    return layout;
}

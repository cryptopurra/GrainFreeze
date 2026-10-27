#include "Parameters.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace juce;

StringArray modDestNames()
{
    return { "None", "Volume", "Grain Size", "Grain Overlap", "Grain Pitch",
             "Scan Speed", "Spectral Mix", "Spectral Smear", "Spectral Hold",
             "Highpass", "Lowpass", "Robotize", "Reducer" };
}

static StringArray resolutionNames()
{
    return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
}

APVTS::ParameterLayout createParameterLayout()
{
    APVTS::ParameterLayout layout;
    auto pct   = [] (float def) { return NormalisableRange<float> (0.0f, 1.0f, 0.0001f); };

    // --- global / source ---
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::freeze, 1 },     "Freeze", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::reset, 1 },      "Reset",  false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::inputGain, 1 },  "Input Gain",  NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::outputGain, 1 }, "Output Gain", NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::dryWet, 1 },     "Dry/Wet", pct (1.f), 1.0f));

    // --- granular core ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainSize, 1 },    "Grain Size",    NormalisableRange<float> (64.f, 8192.f, 1.f, 0.4f), 2048.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainOverlap, 1 }, "Grain Overlap", pct (1.f), 0.75f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainPitch, 1 },   "Grain Pitch",   NormalisableRange<float> (-36.f, 36.f, 0.01f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::scanSpeed, 1 },    "Scan Speed",    NormalisableRange<float> (-2.f, 2.f, 0.001f), 1.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainJitter, 1 },  "Grain Jitter",  pct (0.f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainSpread, 1 },  "Grain Spread",  pct (0.f), 0.3f));

    // --- spectral ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::specMix, 1 },   "Spectral Mix",   pct (0.f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::specSmear, 1 }, "Spectral Smear", pct (0.f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::specHold, 1 },  "Spectral Hold",  pct (0.f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::specBlur, 1 },  "Spectral Blur",  pct (0.f), 0.0f));

    // --- tone / reducer ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::reducer, 1 },  "Reducer",  pct (1.f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::highpass, 1 }, "Highpass", pct (0.f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::lowpass, 1 },  "Lowpass",  pct (1.f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::robotize, 1 }, "Robotize", pct (0.f), 0.0f));

    // --- step gate ---
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { pid::seqOn, 1 },     "Sequencer On", false));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { pid::seqLength, 1 },  "Seq Length", 1, 32, 16));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::seqResolution, 1 }, "Seq Resolution", resolutionNames(), 3));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { pid::seqOffset, 1 },  "Seq Offset", NormalisableRange<float> (-0.5f, 0.5f, 0.001f), 0.f));

    // --- 6 modulation envelopes ---
    for (int n = 1; n <= pid::numEnvelopes; ++n)
    {
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { pid::env (n, "enable"), 1 },   "Env " + String (n) + " Enable", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::env (n, "mode"), 1 },     "Env " + String (n) + " Mode", StringArray { "Smooth", "Step" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::env (n, "loop"), 1 },     "Env " + String (n) + " Loop", StringArray { "One-Shot", "Loop", "Ping-Pong" }, 1));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::env (n, "lengthBars"), 1 }, "Env " + String (n) + " Bars", StringArray { "1/2", "1", "2", "4", "8", "16" }, 2));
        layout.add (std::make_unique<AudioParameterInt>    (ParameterID { pid::env (n, "steps"), 1 },    "Env " + String (n) + " Steps", 2, 32, 16));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { pid::env (n, "depth"), 1 },    "Env " + String (n) + " Depth", NormalisableRange<float> (-1.f, 1.f, 0.001f), 1.f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::env (n, "dest"), 1 },     "Env " + String (n) + " Dest", modDestNames(), 0));
    }

    return layout;
}

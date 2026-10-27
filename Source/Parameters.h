#pragma once
#include <JuceHeader.h>
#include <vector>

// ---------------------------------------------------------------------------
// Parameter model for GrainFreeze.
//
// The control surface mirrors TimeFreeze's *functional* layout exactly
// (original implementation, no proprietary code/art):
//   - main granular + spectral knobs
//   - a FREEZE / RESET pair + input/output gain
//   - 6 modulation envelopes presented as tabs: Time, Pitch, Misc 1..4
//     each with Enable / Mode(Normal|Step) / Bars / Loop(OneShot|Loop|PingPong)
//     / StepCount, and a fixed set of per-destination depth knobs that match
//     the knob row shown under each tab.
//   - a multi-level step sequencer (Length / Offset / Resolution / On).
//   - NEW: a Voice section (drop a sample, time-stretch, sync to DAW tempo,
//     automatic BPM detection) that feeds the same engines.
// ---------------------------------------------------------------------------

enum class ModDest
{
    none = 0,
    speed, pitch, volume, overlap, grainSize, specMix, specHold,
    highpass, lowpass, robotize,
    count
};

const char* modDestShort (ModDest d);   // "spd","pit",...
const char* modDestLabel (ModDest d);   // "Speed","Pitch",...

enum class EnvMode  { normal = 0, step };
enum class LoopMode { oneShot = 0, loop, pingPong };

namespace pid
{
    // ---- global / source ----
    constexpr auto freeze     = "freeze";
    constexpr auto reset      = "reset";
    constexpr auto inputGain  = "inputGain";
    constexpr auto outputGain = "outputGain";
    constexpr auto bypass     = "bypass";
    constexpr auto limiterOn  = "limiterOn";

    // ---- granular core ----
    constexpr auto grainSize    = "grainSize";
    constexpr auto grainOverlap = "grainOverlap";
    constexpr auto grainPitch   = "grainPitch";    // -100..100 (TimeFreeze "Pitch")
    constexpr auto scanSpeed    = "scanSpeed";     // 0..1 "Speed"
    constexpr auto volume       = "volume";        // 0..2
    constexpr auto reducer      = "reducer";       // 0..1

    // ---- spectral ----
    constexpr auto specMix   = "specMix";
    constexpr auto specSmear = "specSmear";
    constexpr auto specHold  = "specHold";

    // ---- tone ----
    constexpr auto highpass  = "highpass";
    constexpr auto lowpass   = "lowpass";
    constexpr auto robotize  = "robotize";

    // ---- step sequencer ----
    constexpr auto seqOn         = "seqOn";
    constexpr auto seqLength     = "seqLength";     // 1..32
    constexpr auto seqResolution = "seqResolution"; // enum
    constexpr auto seqOffset     = "seqOffset";     // -0.5..0.5

    // ---- voice section (new) ----
    constexpr auto voiceEnable  = "voiceEnable";    // use loaded sample as source
    constexpr auto voiceGain    = "voiceGain";      // dB
    constexpr auto stretchSync  = "stretchSync";    // sync stretch to host tempo
    constexpr auto stretchRatio = "stretchRatio";   // manual stretch (sync off)

    // ---- envelopes ----
    constexpr int numEnvelopes = 6;   // 0=Time 1=Pitch 2..5=Misc1..4

    inline juce::String envField (int n, const char* field)
    {
        return "env" + juce::String (n) + "_" + field;
    }
    inline juce::String envDepth (int n, ModDest d)
    {
        return "env" + juce::String (n) + "_dp_" + modDestShort (d);
    }
}

// Tab/envelope metadata (which destinations each envelope drives).
struct EnvSpec
{
    juce::String name;                 // "Time", "Pitch", "Misc 1", ...
    std::vector<ModDest> dests;        // knob row under the tab
};

const std::vector<EnvSpec>& envSpecs();   // size == pid::numEnvelopes

juce::StringArray resolutionNames();
juce::StringArray barsNames();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

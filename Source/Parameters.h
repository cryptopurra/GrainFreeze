#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// Parameter identifiers and the APVTS layout for GrainFreeze.
//
// The set is organised from the behaviour we want (original design):
//   - a granular core that reads from a capture buffer in LIVE or FREEZE mode
//   - a parallel clean STFT spectral stage (smear / hold / freeze)
//   - a reducer (lo-fi) stage
//   - 6 tempo-synced modulation envelopes (smooth OR step), freely routable
//   - a multi-level step gate (sequencer) synced to the host
//   - input/output gain + a safety limiter
// ---------------------------------------------------------------------------

namespace pid
{
    // ---- global / source --------------------------------------------------
    constexpr auto freeze        = "freeze";        // bool: freeze capture buffer
    constexpr auto reset         = "reset";         // bool (momentary): clear buffer
    constexpr auto inputGain     = "inputGain";     // dB
    constexpr auto outputGain    = "outputGain";    // dB
    constexpr auto dryWet        = "dryWet";        // 0..1 (extra vs original)

    // ---- granular core ----------------------------------------------------
    constexpr auto grainSize     = "grainSize";     // samples (or ms-mapped)
    constexpr auto grainOverlap  = "grainOverlap";  // 0..1 density
    constexpr auto grainPitch    = "grainPitch";    // semitones (-36..+36)
    constexpr auto scanSpeed     = "scanSpeed";     // -2..+2 read speed (neg = reverse)
    constexpr auto grainJitter   = "grainJitter";   // 0..1 position randomisation
    constexpr auto grainSpread   = "grainSpread";   // 0..1 stereo spread

    // ---- spectral (clean STFT) -------------------------------------------
    constexpr auto specMix       = "specMix";       // 0..1 grain<->spectral blend
    constexpr auto specSmear     = "specSmear";     // 0..1 magnitude smoothing
    constexpr auto specHold      = "specHold";      // 0..1 freeze magnitudes
    constexpr auto specBlur      = "specBlur";      // 0..1 bin-to-bin diffusion

    // ---- tone / reducer ---------------------------------------------------
    constexpr auto reducer       = "reducer";       // 0..1 (1 = clean)
    constexpr auto highpass      = "highpass";      // 0..1 normalised cutoff
    constexpr auto lowpass       = "lowpass";       // 0..1 normalised cutoff
    constexpr auto robotize      = "robotize";      // 0..1 phase-lock amount

    // ---- step gate (sequencer) -------------------------------------------
    constexpr auto seqOn         = "seqOn";         // bool
    constexpr auto seqLength     = "seqLength";     // 1..32 steps
    constexpr auto seqResolution = "seqResolution"; // enum: 1/4,1/8,1/16,...
    constexpr auto seqOffset     = "seqOffset";     // -0.5..0.5

    // ---- 6 modulation envelopes ------------------------------------------
    // Each env N has: enable, mode(smooth/step), lengthBars, loopMode, depth, dest.
    // IDs are generated as envN_* below.
    constexpr int  numEnvelopes  = 6;

    inline juce::String env (int n, const char* field)
    {
        return "env" + juce::String (n) + "_" + field;
    }
}

// Routing destinations a modulation envelope can target.
enum class ModDest
{
    none = 0,
    volume, grainSize, grainOverlap, grainPitch, scanSpeed,
    specMix, specSmear, specHold,
    highpass, lowpass, robotize, reducer,
    count
};

juce::StringArray modDestNames();

enum class EnvMode  { smooth = 0, step };
enum class LoopMode { oneShot = 0, loop, pingPong };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

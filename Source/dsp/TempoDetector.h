#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// TempoDetector
//
// Offline BPM estimate for a loaded sample. Method (original, standard DSP):
//   1. Build an onset-strength envelope from short-time energy differences
//      (half-wave rectified positive energy flux).
//   2. Autocorrelate the envelope over the lag range that corresponds to
//      60..180 BPM and pick the strongest periodicity.
//   3. Fold the result into a musical range and return the BPM.
//
// Good enough to auto-match a dropped loop to the project tempo; the user can
// override the detected value in the UI.
// ---------------------------------------------------------------------------

class TempoDetector
{
public:
    // Returns an estimated BPM in [60,180], or 0 if it can't decide.
    static double estimateBpm (const juce::AudioBuffer<float>& buffer, double sampleRate);
};

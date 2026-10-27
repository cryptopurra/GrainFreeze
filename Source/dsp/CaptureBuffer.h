#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// CaptureBuffer
//
// Holds the material the engines read from. Two sources:
//
//   LIVE INPUT  : a stereo ring buffer that is always being written from the
//                 plugin input. Freeze parks the read anchor (time-freeze);
//                 unfrozen, the anchor tracks the write head (live granular).
//
//   LOADED VOICE: a dropped audio sample. A playhead moves through it, advanced
//                 by the processor at a stretch ratio (for tempo-sync time-
//                 stretch). Freeze parks the playhead too.
//
// Which source is active is chosen by setUseSample(). Engines call readSample()
// and getAnchor() without needing to know which source is live.
// ---------------------------------------------------------------------------

class CaptureBuffer
{
public:
    void prepare (double sampleRate, int maxBlock, double seconds = 8.0);
    void reset();

    // --- live input ---
    void writeBlock (const juce::AudioBuffer<float>& in);

    // --- loaded sample (voice) ---
    // Replaces the sample content; `fileSampleRate` is resampled to host rate.
    void loadSample (const juce::AudioBuffer<float>& src, double fileSampleRate);
    bool hasSample() const noexcept { return sampleLength > 0; }
    int  getSampleLength() const noexcept { return sampleLength; }
    void clearSample() noexcept { sampleLength = 0; samplePlayhead = 0.0; }

    void setUseSample (bool use) noexcept { useSample = use; }
    bool usingSample() const noexcept { return useSample && hasSample(); }

    // Advance the sample playhead by `delta` samples (stretch * blockSize),
    // called once per block by the processor. Wraps over the sample length.
    void advanceSamplePlayhead (double delta) noexcept;
    // Sets the playhead absolutely (tempo-locked sync); wraps over the sample length.
    void setSamplePlayhead (double pos) noexcept;
    double getSamplePlayhead() const noexcept { return samplePlayhead; }

    // --- freeze (applies to whichever source is active) ---
    void setFrozen (bool shouldFreeze) noexcept;
    bool isFrozen() const noexcept { return frozen; }

    // --- reads (engines) ---
    float  readSample (int channel, double pos) const noexcept;
    double getAnchor() const noexcept;

    int getNumChannels() const noexcept { return 2; }
    int getLength()      const noexcept { return usingSample() ? sampleLength : length; }

private:
    float wrapRead (const juce::AudioBuffer<float>& buf, int len, int channel, double pos) const noexcept;

    // live ring
    juce::AudioBuffer<float> ring;
    int    length       = 0;
    int    writeHead    = 0;
    double frozenAnchor = 0.0;

    // loaded sample
    juce::AudioBuffer<float> sample;
    int    sampleLength   = 0;
    double samplePlayhead = 0.0;
    double sampleFrozenAt = 0.0;

    bool frozen    = false;
    bool useSample = false;
};

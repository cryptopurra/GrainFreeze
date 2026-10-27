#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// CaptureBuffer
//
// A stereo circular buffer that is ALWAYS being written from the input.
// The grain + spectral engines read from it. A single "frozen" flag decides
// what the read heads see:
//
//   LIVE   (frozen == false): the write head keeps advancing, so grains read
//          recent input -> the effect works as a live granular processor.
//   FREEZE (frozen == true) : the write head is parked, so grains loop over a
//          fixed window -> classic time-freeze.
//
// This is the key behaviour we confirmed at runtime: Freeze is a *source*
// switch, not an on/off for the engine. The engine runs identically in both
// states; only the read material changes.
// ---------------------------------------------------------------------------

class CaptureBuffer
{
public:
    void prepare (double sampleRate, int maxBlock, double seconds = 8.0);
    void reset();

    // Push one block of input (always called, every block).
    void writeBlock (const juce::AudioBuffer<float>& in);

    void setFrozen (bool shouldFreeze) noexcept;
    bool isFrozen() const noexcept { return frozen; }

    // Linear-interpolated read at an absolute fractional sample position.
    // `pos` is measured in samples back from the current freeze anchor / write
    // head; engines manage their own read phases and call this per grain.
    float readSample (int channel, double pos) const noexcept;

    int    getNumChannels() const noexcept { return buffer.getNumChannels(); }
    int    getLength()      const noexcept { return length; }
    double getWritePos()    const noexcept { return (double) writeHead; }

    // Anchor = the read origin. In FREEZE it is fixed at the freeze moment;
    // in LIVE it tracks the write head so grains follow the incoming signal.
    double getAnchor()      const noexcept { return frozen ? frozenAnchor : (double) writeHead; }

private:
    juce::AudioBuffer<float> buffer;
    int    length     = 0;
    int    writeHead  = 0;
    bool   frozen     = false;
    double frozenAnchor = 0.0;

    float wrapRead (int channel, double pos) const noexcept;
};

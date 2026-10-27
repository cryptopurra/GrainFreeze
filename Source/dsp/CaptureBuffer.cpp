#include "CaptureBuffer.h"

void CaptureBuffer::prepare (double sampleRate, int /*maxBlock*/, double seconds)
{
    length = juce::jmax (1, (int) std::ceil (sampleRate * seconds));
    buffer.setSize (2, length, false, true, true);
    reset();
}

void CaptureBuffer::reset()
{
    buffer.clear();
    writeHead    = 0;
    frozen       = false;
    frozenAnchor = 0.0;
}

void CaptureBuffer::writeBlock (const juce::AudioBuffer<float>& in)
{
    const int chans = juce::jmin (buffer.getNumChannels(), in.getNumChannels());
    const int n     = in.getNumSamples();

    // Always write, even while frozen: the newest input stays available the
    // instant the user leaves FREEZE, so there is no silent gap.
    for (int s = 0; s < n; ++s)
    {
        const int idx = (writeHead + s) % length;
        for (int c = 0; c < chans; ++c)
            buffer.setSample (c, idx, in.getSample (c, s));
    }
    writeHead = (writeHead + n) % length;
}

void CaptureBuffer::setFrozen (bool shouldFreeze) noexcept
{
    if (shouldFreeze && ! frozen)
        frozenAnchor = (double) writeHead;   // latch the window origin
    frozen = shouldFreeze;
}

float CaptureBuffer::wrapRead (int channel, double pos) const noexcept
{
    // Wrap into [0, length) with fractional interpolation.
    double p = std::fmod (pos, (double) length);
    if (p < 0.0) p += (double) length;

    const int   i0 = (int) p;
    const int   i1 = (i0 + 1) % length;
    const float f  = (float) (p - (double) i0);

    const float a = buffer.getSample (channel, i0);
    const float b = buffer.getSample (channel, i1);
    return a + (b - a) * f;
}

float CaptureBuffer::readSample (int channel, double pos) const noexcept
{
    channel = juce::jlimit (0, buffer.getNumChannels() - 1, channel);
    return wrapRead (channel, pos);
}

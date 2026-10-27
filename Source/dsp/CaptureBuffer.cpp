#include "CaptureBuffer.h"

void CaptureBuffer::prepare (double sampleRate, int /*maxBlock*/, double seconds)
{
    length = juce::jmax (1, (int) std::ceil (sampleRate * seconds));
    ring.setSize (2, length, false, true, true);
    reset();
}

void CaptureBuffer::reset()
{
    ring.clear();
    writeHead      = 0;
    frozen         = false;
    frozenAnchor   = 0.0;
    samplePlayhead = 0.0;
    sampleFrozenAt = 0.0;
}

void CaptureBuffer::writeBlock (const juce::AudioBuffer<float>& in)
{
    const int chans = juce::jmin (2, in.getNumChannels());
    const int n     = in.getNumSamples();
    for (int s = 0; s < n; ++s)
    {
        const int idx = (writeHead + s) % length;
        for (int c = 0; c < chans; ++c)
            ring.setSample (c, idx, in.getSample (c, s));
        if (chans == 1) ring.setSample (1, idx, in.getSample (0, s));
    }
    writeHead = (writeHead + n) % length;
}

void CaptureBuffer::loadSample (const juce::AudioBuffer<float>& src, double fileSampleRate)
{
    // Resample to the host rate with a simple linear interpolation. (A higher
    // quality resampler can replace this later.)
    const double hostRate = (length > 0) ? (double) length / 8.0 : fileSampleRate;
    const double ratio    = fileSampleRate / hostRate;
    const int srcLen      = src.getNumSamples();
    const int outLen      = juce::jmax (1, (int) std::round (srcLen / ratio));

    sample.setSize (2, outLen, false, true, true);
    const int srcCh = src.getNumChannels();

    for (int c = 0; c < 2; ++c)
    {
        const int sc = juce::jmin (c, srcCh - 1);
        for (int i = 0; i < outLen; ++i)
        {
            const double sp = (double) i * ratio;
            const int    i0 = (int) sp;
            const int    i1 = juce::jmin (i0 + 1, srcLen - 1);
            const float  f  = (float) (sp - i0);
            const float  a  = src.getSample (sc, juce::jlimit (0, srcLen - 1, i0));
            const float  b  = src.getSample (sc, i1);
            sample.setSample (c, i, a + (b - a) * f);
        }
    }
    sampleLength   = outLen;
    samplePlayhead = 0.0;
}

void CaptureBuffer::advanceSamplePlayhead (double delta) noexcept
{
    if (! usingSample() || frozen) return;
    samplePlayhead += delta;
    if (sampleLength > 0)
    {
        while (samplePlayhead >= sampleLength) samplePlayhead -= sampleLength;
        while (samplePlayhead < 0.0)           samplePlayhead += sampleLength;
    }
}

void CaptureBuffer::setSamplePlayhead (double pos) noexcept
{
    if (! usingSample() || frozen || sampleLength <= 0) return;
    samplePlayhead = std::fmod (pos, (double) sampleLength);
    if (samplePlayhead < 0.0) samplePlayhead += sampleLength;
}

void CaptureBuffer::setFrozen (bool shouldFreeze) noexcept
{
    if (shouldFreeze && ! frozen)
    {
        frozenAnchor   = (double) writeHead;
        sampleFrozenAt = samplePlayhead;
    }
    frozen = shouldFreeze;
}

float CaptureBuffer::wrapRead (const juce::AudioBuffer<float>& buf, int len, int channel, double pos) const noexcept
{
    if (len <= 0) return 0.0f;
    double p = std::fmod (pos, (double) len);
    if (p < 0.0) p += (double) len;

    const int   i0 = (int) p;
    const int   i1 = (i0 + 1) % len;
    const float f  = (float) (p - (double) i0);
    const float a  = buf.getSample (channel, i0);
    const float b  = buf.getSample (channel, i1);
    return a + (b - a) * f;
}

float CaptureBuffer::readSample (int channel, double pos) const noexcept
{
    channel = juce::jlimit (0, 1, channel);
    if (usingSample())
        return wrapRead (sample, sampleLength, channel, pos);
    return wrapRead (ring, length, channel, pos);
}

double CaptureBuffer::getAnchor() const noexcept
{
    if (usingSample())
        return frozen ? sampleFrozenAt : samplePlayhead;
    return frozen ? frozenAnchor : (double) writeHead;
}

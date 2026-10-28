#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// CaptureBuffer
//
//   ring   : stereo ring, ALWAYS recording the input (so LIVE is always right).
//   frozen : a snapshot of the last `frozenLen` samples, copied the instant
//            Freeze engages. Frozen grains read this snapshot, so un-freezing
//            never sees stale or half-overwritten audio.
// ---------------------------------------------------------------------------
class CaptureBuffer
{
public:
    void prepare (double sampleRate, double ringSeconds = 4.0, double frozenSeconds = 1.0)
    {
        ringLen   = juce::jmax (4096, (int) std::ceil (sampleRate * ringSeconds));
        frozenLen = juce::jmax (4096, (int) std::ceil (sampleRate * frozenSeconds));
        ring.setSize (2, ringLen, false, true, true);
        frozen.setSize (2, frozenLen, false, true, true);
        reset();
    }

    void reset()
    {
        ring.clear(); frozen.clear();
        writeHead = 0; recorded = 0; frozenValid = frozenLen;
    }

    void writeBlock (const juce::AudioBuffer<float>& in)
    {
        const int n = in.getNumSamples();
        const int chans = in.getNumChannels();
        for (int c = 0; c < 2; ++c)
        {
            const float* src = in.getReadPointer (juce::jmin (c, chans - 1));
            float* dst = ring.getWritePointer (c);
            int w = writeHead;
            for (int i = 0; i < n; ++i)
            {
                dst[w] = src[i];
                if (++w >= ringLen) w = 0;
            }
        }
        writeHead = (writeHead + n) % ringLen;
        recorded = juce::jmin (ringLen, recorded + n);
    }

    // Copy the most recent (up to frozenLen) samples into the snapshot, oldest first.
    // Only audio that was really recorded counts, so freezing right at the start
    // of a playback never scans through empty silence.
    void snapshot()
    {
        frozenValid = juce::jlimit (64, frozenLen, recorded);
        for (int c = 0; c < 2; ++c)
        {
            const float* src = ring.getReadPointer (c);
            float* dst = frozen.getWritePointer (c);
            int r = ((writeHead - frozenValid) % ringLen + ringLen) % ringLen;
            for (int i = 0; i < frozenValid; ++i)
            {
                dst[i] = src[r];
                if (++r >= ringLen) r = 0;
            }
        }
    }

    float readLive (int ch, double pos) const noexcept   { return wrapRead (ring, ringLen, ch, pos); }
    float readFrozen (int ch, double pos) const noexcept { return wrapRead (frozen, frozenValid, ch, pos); }

    int getWriteHead() const noexcept { return writeHead; }
    int getFrozenLen() const noexcept { return frozenValid; }
    int getRingLen()   const noexcept { return ringLen; }

private:
    static float wrapRead (const juce::AudioBuffer<float>& buf, int len, int ch, double pos) noexcept
    {
        double p = std::fmod (pos, (double) len);
        if (p < 0.0) p += (double) len;
        const int i0 = (int) p;
        const int i1 = (i0 + 1 >= len) ? 0 : i0 + 1;
        const float f = (float) (p - (double) i0);
        const float* d = buf.getReadPointer (ch);
        return d[i0] + (d[i1] - d[i0]) * f;
    }

    juce::AudioBuffer<float> ring, frozen;
    int ringLen = 0, frozenLen = 0, frozenValid = 0, writeHead = 0, recorded = 0;
};

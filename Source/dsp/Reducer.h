#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// Reducer  --  a lo-fi stage (bit-depth + sample-rate reduction).
//
// amount == 1.0 -> clean (bypass). As amount drops, bits are reduced and the
// signal is sample-and-held at a lower effective rate, giving the gritty
// "reducer" character. Original implementation.
// ---------------------------------------------------------------------------

class Reducer
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { holdL = holdR = 0.0f; phase = 0.0f; }

    void setAmount (float a) noexcept { amount = juce::jlimit (0.f, 1.f, a); }

    void process (juce::AudioBuffer<float>& buf, int numSamples) noexcept
    {
        if (amount >= 0.999f) return;             // clean

        // Map amount -> bits (16..4) and downsample factor (1..~32).
        const float bits  = juce::jmap (amount, 4.0f, 16.0f);
        const float steps = std::pow (2.0f, bits);
        const float ratio = juce::jmap (amount, 32.0f, 1.0f); // low amount -> big step

        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : L;

        for (int s = 0; s < numSamples; ++s)
        {
            phase += 1.0f / ratio;
            if (phase >= 1.0f)
            {
                phase -= 1.0f;
                holdL = std::round (L[s] * steps) / steps;
                holdR = std::round (R[s] * steps) / steps;
            }
            L[s] = holdL;
            R[s] = holdR;
        }
    }

private:
    double sr = 44100.0;
    float  amount = 1.0f;
    float  holdL = 0.f, holdR = 0.f, phase = 0.f;
};

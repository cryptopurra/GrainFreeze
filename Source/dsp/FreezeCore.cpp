#include "FreezeCore.h"
using namespace juce;

void FreezeCore::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    capture.prepare (sr);
    grain.prepare (sr);   grain.setCapture (&capture);
    spectral.prepare (sr); spectral.setCapture (&capture);
    reducerFx.prepare (sr);

    grainBuf.setSize (2, maxBlock);
    specBuf.setSize  (2, maxBlock);
    dryBuf.setSize   (2, maxBlock);

    dsp::ProcessSpec spec { sr, (uint32) maxBlock, 2 };
    hp.prepare (spec); hp.setType (dsp::StateVariableTPTFilterType::highpass);
    lp.prepare (spec); lp.setType (dsp::StateVariableTPTFilterType::lowpass);
    limiter.prepare (spec); limiter.setThreshold (-0.5f); limiter.setRelease (50.0f);
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) latencySamples());

    wetSm.reset (sr, 0.03);
    wetSm.setCurrentAndTargetValue (1.0f);
    lastIn = lastOut = lastVol = 1.0f;
    wasFrozen = wasSpecOn = false;
}

void FreezeCore::process (AudioBuffer<float>& buffer, const FreezeParams& p)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)            // mono host: work on a stereo scratch copy
    {
        AudioBuffer<float> st (2, n);
        st.copyFrom (0, 0, buffer, 0, 0, n);
        st.copyFrom (1, 0, buffer, 0, 0, n);
        process (st, p);
        buffer.copyFrom (0, 0, st, 0, 0, n);
        return;
    }

    // --- input gain ---
    {
        const float g = Decibels::decibelsToGain (p.inGainDb);
        buffer.applyGainRamp (0, n, lastIn, g);
        lastIn = g;
    }
    for (int c = 0; c < 2; ++c) inLevel[c].store (buffer.getMagnitude (c, 0, n));

    // --- latency-matched dry copy ---
    for (int c = 0; c < 2; ++c)
    {
        const float* s = buffer.getReadPointer (c);
        float* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, s[i]);
            d[i] = dryDelay.popSample (c);
        }
    }

    // --- record, then (un)freeze ---
    capture.writeBlock (buffer);
    if (p.frozen && ! wasFrozen) capture.snapshot();
    wasFrozen = p.frozen;
    grain.setFrozen (p.frozen);
    spectral.setFrozen (p.frozen);

    // --- engines ---
    grain.setGrainSize (p.grainSize);
    grain.setOverlap (p.overlap);
    grain.setPitch (p.pitchSemis);
    grain.setSpeed (p.speed);
    grain.process (grainBuf, n);

    const bool specOn = p.specMix > 0.001f;
    if (specOn)
    {
        if (! wasSpecOn) spectral.reset(), spectral.setFrozen (p.frozen);
        spectral.setSmear (p.smear);
        spectral.setHold (p.hold);
        spectral.setRobotize (p.robotize);
        spectral.setSpeed (p.speed);
        spectral.process (specBuf, n);
    }
    else
        specBuf.clear();
    wasSpecOn = specOn;

    for (int c = 0; c < 2; ++c)
    {
        float* o = buffer.getWritePointer (c);
        const float* g = grainBuf.getReadPointer (c);
        const float* sp = specBuf.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            o[i] = g[i] * (1.0f - p.specMix) + sp[i] * p.specMix;
    }

    // --- reducer, filters, volume ---
    reducerFx.setAmount (p.reducer);
    reducerFx.process (buffer, n);

    hp.setCutoffFrequency (jmap (p.highpass, 20.0f, 2000.0f));
    lp.setCutoffFrequency (jmap (p.lowpass, 500.0f, 20000.0f));
    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        if (p.highpass > 0.001f) hp.process (ctx);
        if (p.lowpass  < 0.999f) lp.process (ctx);
    }
    buffer.applyGainRamp (0, n, lastVol, p.volume);
    lastVol = p.volume;

    // --- LIVE / FREEZE wet switch ---
    wetSm.setTargetValue ((p.frozen || p.live) ? 1.0f : 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float w = wetSm.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            float* o = buffer.getWritePointer (c);
            o[i] = o[i] * w + dryBuf.getSample (c, i) * (1.0f - w);
        }
    }

    // --- output ---
    {
        const float g = Decibels::decibelsToGain (p.outGainDb);
        buffer.applyGainRamp (0, n, lastOut, g);
        lastOut = g;
    }
    if (p.limiter)
    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        limiter.process (ctx);
    }
    for (int c = 0; c < 2; ++c) outLevel[c].store (buffer.getMagnitude (c, 0, n));
}

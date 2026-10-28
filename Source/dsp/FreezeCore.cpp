#include "FreezeCore.h"
using namespace juce;

void FreezeCore::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = jmax (16, maxBlockSize);
    capture.prepare (sr);
    grain.prepare (sr);   grain.setCapture (&capture);
    for (auto& sp : spectral) sp.prepare();
    reducerFx.prepare (sr);

    grainBuf.setSize (2, maxBlock);
    specBuf.setSize  (2, maxBlock);
    dlyBuf.setSize   (2, maxBlock);
    dryBuf.setSize   (2, maxBlock);

    dsp::ProcessSpec spec { sr, (uint32) maxBlock, 2 };
    hp.prepare (spec); hp.setType (dsp::StateVariableTPTFilterType::highpass);
    lp.prepare (spec); lp.setType (dsp::StateVariableTPTFilterType::lowpass);
    limiter.prepare (spec); limiter.setThreshold (-0.5f); limiter.setRelease (50.0f);
    dryDelay.prepare (spec);   dryDelay.setDelay ((float) latencySamples());
    grainDelay.prepare (spec); grainDelay.setDelay ((float) latencySamples());

    wetSm.reset (sr, 0.03);
    wetSm.setCurrentAndTargetValue (1.0f);
    lastIn = lastOut = lastVol = 1.0f;
    wasFrozen = wasSpecOn = false;
}

void FreezeCore::process (AudioBuffer<float>& buffer, const FreezeParams& p)
{
    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)            // mono host: work on a stereo scratch copy
    {
        AudioBuffer<float> st (2, total);
        st.copyFrom (0, 0, buffer, 0, 0, total);
        st.copyFrom (1, 0, buffer, 0, 0, total);
        process (st, p);
        buffer.copyFrom (0, 0, st, 0, 0, total);
        return;
    }
    // Hosts may exceed the prepared block size: render in safe chunks.
    for (int off = 0; off < total; off += maxBlock)
    {
        const int len = jmin (maxBlock, total - off);
        AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), 2, off, len);
        processChunk (sub, p);
    }
}

void FreezeCore::processChunk (AudioBuffer<float>& buffer, const FreezeParams& p)
{
    const int n = buffer.getNumSamples();

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

    // --- grain engine (live or frozen) ---
    grain.setGrainSize (p.grainSize);
    grain.setOverlap (p.overlap);
    grain.setPitch (p.pitchSemis);
    grain.setSpeed (p.speed);
    grain.process (grainBuf, n);

    // --- spectral stage on the grain output; the grain path is delayed to match ---
    const bool specOn = p.specMix > 0.001f;
    for (int c = 0; c < 2; ++c)
    {
        const float* g = grainBuf.getReadPointer (c);
        float* d = dlyBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            grainDelay.pushSample (c, g[i]);
            d[i] = grainDelay.popSample (c);
        }
        if (specOn)
        {
            if (! wasSpecOn) spectral[c].reset();
            spectral[c].setSmear (p.smear);
            spectral[c].setHold (p.hold);
            spectral[c].setRobotize (p.robotize);
            spectral[c].process (g, specBuf.getWritePointer (c), n);
        }
        float* o = buffer.getWritePointer (c);
        const float* sp = specBuf.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            o[i] = specOn ? d[i] * (1.0f - p.specMix) + sp[i] * p.specMix : d[i];
    }
    wasSpecOn = specOn;

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

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/TempoDetector.h"

using namespace juce;

GrainFreezeAudioProcessor::GrainFreezeAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    // Defaults: sequencer fully open; Time envelope = the classic decaying ramp.
    for (int i = 0; i < 32; ++i) seq.setStep (i, StepSequencer::maxLevel);
    std::vector<juce::Point<float>> decay;
    for (float x : { 0.0f, 0.08f, 0.18f, 0.3f, 0.45f, 0.62f, 0.8f, 1.0f })
        decay.push_back ({ x, std::pow (1.0f - x, 2.2f) });
    mods[0].setCurve (decay);
}

bool GrainFreezeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void GrainFreezeAudioProcessor::prepareToPlay (double sr, int maxBlock)
{
    sampleRate = sr;

    capture.prepare (sr, maxBlock, 8.0);
    grain.prepare (sr, maxBlock);
    grain.setCaptureBuffer (&capture);
    spectral.prepare (sr, maxBlock);
    spectral.setCaptureBuffer (&capture);
    reducerFx.prepare (sr);

    grainBuf.setSize (2, maxBlock);
    specBuf.setSize  (2, maxBlock);
    dryBuf.setSize   (2, maxBlock);

    dsp::ProcessSpec spec { sr, (uint32) maxBlock, 2 };
    hpFilter.prepare (spec); hpFilter.setType (dsp::StateVariableTPTFilterType::highpass);
    lpFilter.prepare (spec); lpFilter.setType (dsp::StateVariableTPTFilterType::lowpass);
    limiter.prepare (spec);  limiter.setThreshold (-0.5f); limiter.setRelease (50.0f);

    gateSmoothed.reset (sr, 0.003);
    gateSmoothed.setCurrentAndTargetValue (1.0f);
    wetSm.reset (sr, 0.03);
    wetSm.setCurrentAndTargetValue (1.0f);

    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) spectral.getLatencySamples());
    wasSpecOn = false;
    lastInGain = lastOutGain = lastVoiceGain = 1.0f;

    setLatencySamples (spectral.getLatencySamples());
}

bool GrainFreezeAudioProcessor::loadSampleFile (const File& file)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    AudioBuffer<float> tmp ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&tmp, 0, (int) reader->lengthInSamples, 0, true, true);
    const double fileRate = reader->sampleRate;

    const double bpm = TempoDetector::estimateBpm (tmp, fileRate);

    // Briefly suspend the audio thread to install the resampled sample safely.
    suspendProcessing (true);
    capture.loadSample (tmp, fileRate);
    suspendProcessing (false);

    {
        // Min/max overview for the waveform display.
        const int len = tmp.getNumSamples(), chans = tmp.getNumChannels();
        constexpr int cols = 1200;
        std::vector<float> pk ((size_t) cols * 2, 0.0f);
        for (int i = 0; i < cols && len > 0; ++i)
        {
            const int a = (int) ((int64) i * len / cols), b = jmax (a + 1, (int) ((int64) (i + 1) * len / cols));
            float lo = 0.0f, hi = 0.0f;
            for (int x = a; x < b && x < len; ++x)
            {
                float v = 0.0f;
                for (int c = 0; c < chans; ++c) v += tmp.getSample (c, x);
                v /= (float) jmax (1, chans);
                lo = jmin (lo, v); hi = jmax (hi, v);
            }
            pk[(size_t) i * 2] = lo; pk[(size_t) i * 2 + 1] = hi;
        }
        const ScopedLock sl (peakLock);
        peaks = std::move (pk);
        sampleDuration = fileRate > 0.0 ? (double) len / fileRate : 0.0;
    }
    ++sampleVersion;

    if (bpm > 0.0) detectedBpm.store (bpm);
    sampleName = file.getFileName();
    return true;
}

void GrainFreezeAudioProcessor::applyModulation (double ppq)
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    tgt.grainSize  = get (pid::grainSize);
    tgt.overlap    = get (pid::grainOverlap);
    tgt.pitchSemis = get (pid::grainPitch) / 100.0f * 24.0f;   // -100..100 -> +/-24 semis
    tgt.scanSpeed  = get (pid::scanSpeed);
    tgt.volume     = get (pid::volume);
    tgt.reducer    = get (pid::reducer);
    tgt.specMix    = get (pid::specMix);
    tgt.specSmear  = get (pid::specSmear);
    tgt.specHold   = get (pid::specHold);
    tgt.highpass   = get (pid::highpass);
    tgt.lowpass    = get (pid::lowpass);
    tgt.robotize   = get (pid::robotize);

    const auto& specs = envSpecs();
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto& m = mods[(size_t) n];
        // sync per-block control from params
        m.setEnabled ((bool) (get (pid::envField (n, "enable").toRawUTF8()) > 0.5f));
        if (! m.isEnabled()) continue;
        m.setMode     ((EnvMode)  (int) get (pid::envField (n, "mode").toRawUTF8()));
        m.setLoopMode ((LoopMode) (int) get (pid::envField (n, "loop").toRawUTF8()));
        m.setSteps    ((int) get (pid::envField (n, "steps").toRawUTF8()));
        const int barsIdx = (int) get (pid::envField (n, "bars").toRawUTF8());
        const double barsVals[] = { 0.5, 1, 2, 4, 8, 16 };
        m.setBars (barsVals[jlimit (0, 5, barsIdx)]);

        const float base = m.evaluate (ppq);   // [-1,1]

        for (auto d : specs[(size_t) n].dests)
        {
            const float depth = get (pid::envDepth (n, d).toRawUTF8());
            const float v = base * depth;
            switch (d)
            {
                // Speed is an absolute target: the curve (0..100%) scales the knob.
                case ModDest::speed:     tgt.scanSpeed  *= 1.0f - std::abs (depth) * (1.0f - m.evaluate01 (ppq)); break;
                case ModDest::pitch:     tgt.pitchSemis += v * 24.0f;             break;
                case ModDest::volume:    tgt.volume     += v;                     break;
                case ModDest::overlap:   tgt.overlap    += v;                     break;
                case ModDest::grainSize: tgt.grainSize  *= std::pow (2.0f, v);    break;
                case ModDest::specMix:   tgt.specMix    += v;                     break;
                case ModDest::specHold:  tgt.specHold   += v;                     break;
                case ModDest::highpass:  tgt.highpass   += v;                     break;
                case ModDest::lowpass:   tgt.lowpass    += v;                     break;
                case ModDest::robotize:  tgt.robotize   += v;                     break;
                default: break;
            }
        }
    }

    tgt.grainSize = jlimit (128.f, 4096.f, tgt.grainSize);
    tgt.overlap   = jlimit (0.f, 1.f, tgt.overlap);
    tgt.scanSpeed = jlimit (0.f, 2.f, tgt.scanSpeed);
    tgt.volume    = jlimit (0.f, 2.f, tgt.volume);
    tgt.reducer   = jlimit (0.f, 1.f, tgt.reducer);
    tgt.specMix   = jlimit (0.f, 1.f, tgt.specMix);
    tgt.specSmear = jlimit (0.f, 1.f, tgt.specSmear);
    tgt.specHold  = jlimit (0.f, 1.f, tgt.specHold);
    tgt.highpass  = jlimit (0.f, 1.f, tgt.highpass);
    tgt.lowpass   = jlimit (0.f, 1.f, tgt.lowpass);
    tgt.robotize  = jlimit (0.f, 1.f, tgt.robotize);
}

void GrainFreezeAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    if (get (pid::bypass) > 0.5f)
        return;

    // --- transport ---
    double ppq = 0.0, bpm = 120.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto p = pos->getPpqPosition()) { ppq = *p; havePpq = true; }
            if (auto b = pos->getBpm())         bpm = *b;
            playing = pos->getIsPlaying();
        }
    projectBpm.store (bpm);

    // --- input gain (ramped per block) ---
    {
        const float g = Decibels::decibelsToGain (get (pid::inputGain));
        buffer.applyGainRamp (0, n, lastInGain, g);
        lastInGain = g;
    }
    for (int c = 0; c < 2; ++c)
        inLevel[c].store (buffer.getMagnitude (jmin (c, buffer.getNumChannels() - 1), 0, n), std::memory_order_relaxed);

    // Latency-matched dry copy for the "effect off" path.
    dryBuf.makeCopyOf (buffer, true);
    for (int c = 0; c < dryBuf.getNumChannels(); ++c)
    {
        auto* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, d[i]);
            d[i] = dryDelay.popSample (c);
        }
    }

    // --- source: live ring (parked while frozen) or loaded voice ---
    const bool voiceOn = get (pid::voiceEnable) > 0.5f && capture.hasSample();
    capture.setUseSample (voiceOn);

    if (! capture.isFrozen())
        capture.writeBlock (buffer);

    double anchorRate = 1.0;
    if (voiceOn)
    {
        const bool sync = get (pid::stretchSync) > 0.5f;
        const double ratio = jlimit (0.1, 8.0, sync ? projectBpm.load() / jmax (1.0, detectedBpm.load())
                                                    : (double) get (pid::stretchRatio));
        anchorRate = ratio;

        if (! capture.isFrozen())
        {
            if (sync && playing && havePpq)
            {
                // Lock the loop to the bar grid: position = beats * (sample rate / sample tempo).
                const double startPos = ppq * 60.0 / jmax (1.0, detectedBpm.load()) * sampleRate;
                capture.setSamplePlayhead (startPos + ratio * (double) n);
            }
            else
                capture.advanceSamplePlayhead (ratio * (double) n);
        }
    }

    const bool wantFreeze = get (pid::freeze) > 0.5f;
    capture.setFrozen (wantFreeze);
    frozenUI.store (wantFreeze, std::memory_order_relaxed);
    freezeSeconds.store (wantFreeze ? freezeSeconds.load() + (double) n / sampleRate : 0.0, std::memory_order_relaxed);
    samplePlayNorm.store (voiceOn ? (float) (capture.getSamplePlayhead() / (double) jmax (1, capture.getSampleLength())) : -1.0f,
                          std::memory_order_relaxed);

    if (resetRequested.exchange (false) || get (pid::reset) > 0.5f)
    {
        grain.restartScan();
        spectral.restartScan();
        freezeSeconds.store (0.0);
    }

    // --- modulation ---
    applyModulation (ppq);

    // --- grain engine ---
    grain.setGrainSize (tgt.grainSize);
    grain.setOverlap   (tgt.overlap);
    grain.setPitch     (tgt.pitchSemis);
    grain.setScanSpeed (tgt.scanSpeed);
    grain.setAnchorRate (anchorRate);
    grain.setJitter    (wantFreeze ? 0.08f : 0.0f);
    grain.setSpread    (0.3f);
    grain.process (grainBuf, n);

    // --- spectral engine (only while it is audible) ---
    const bool specOn = tgt.specMix > 0.001f;
    if (specOn)
    {
        if (! wasSpecOn) spectral.reset();
        spectral.setSmear (tgt.specSmear);
        spectral.setHold  (tgt.specHold);
        spectral.setBlur  (0.0f);
        spectral.setRobotize (tgt.robotize);
        spectral.setScanSpeed (tgt.scanSpeed);
        spectral.setAnchorRate (anchorRate);
        spectral.process (specBuf, n);
    }
    else
        specBuf.clear();
    wasSpecOn = specOn;

    // --- crossfade grain <-> spectral ---
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* o = buffer.getWritePointer (c);
        auto* g = grainBuf.getReadPointer (jmin (c, grainBuf.getNumChannels() - 1));
        auto* sp = specBuf.getReadPointer (jmin (c, specBuf.getNumChannels() - 1));
        const float mix = tgt.specMix;
        for (int i = 0; i < n; ++i)
            o[i] = g[i] * (1.0f - mix) + sp[i] * mix;
    }

    // --- reducer ---
    reducerFx.setAmount (tgt.reducer);
    reducerFx.process (buffer, n);

    // --- filters ---
    hpFilter.setCutoffFrequency (jmap (tgt.highpass, 20.0f, 2000.0f));
    lpFilter.setCutoffFrequency (jmap (tgt.lowpass, 500.0f, 20000.0f));
    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        if (tgt.highpass > 0.001f) hpFilter.process (ctx);
        if (tgt.lowpass  < 0.999f) lpFilter.process (ctx);
    }

    // --- voice gain ---
    if (voiceOn)
    {
        const float g = Decibels::decibelsToGain (get (pid::voiceGain));
        buffer.applyGainRamp (0, n, lastVoiceGain, g);
        lastVoiceGain = g;
    }

    // --- step gate + volume ---
    seq.setEnabled    (get (pid::seqOn) > 0.5f);
    seq.setLength     ((int) get (pid::seqLength));
    seq.setResolution ((StepSequencer::Resolution) (int) get (pid::seqResolution));
    seq.setOffset     (get (pid::seqOffset));

    for (int i = 0; i < n; ++i)
    {
        const double localPpq = ppq + (double) i * (bpm / 60.0) / sampleRate;
        gateSmoothed.setTargetValue (seq.gateAt (localPpq));
        const float g = gateSmoothed.getNextValue() * tgt.volume;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.getWritePointer (c)[i] *= g;
    }
    currentStep.store (seq.isEnabled()
        ? seq.stepIndexAt (ppq + (double) (n - 1) * (bpm / 60.0) / sampleRate)
        : -1, std::memory_order_relaxed);

    // --- LIVE switch: effect on live playback vs. clean (latency-matched) input ---
    const bool wet = wantFreeze || voiceOn || get (pid::live) > 0.5f;
    wetSm.setTargetValue (wet ? 1.0f : 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float w = wetSm.getNextValue();
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* o = buffer.getWritePointer (c);
            o[i] = o[i] * w + dryBuf.getSample (jmin (c, dryBuf.getNumChannels() - 1), i) * (1.0f - w);
        }
    }

    // --- output gain + limiter ---
    {
        const float g = Decibels::decibelsToGain (get (pid::outputGain));
        buffer.applyGainRamp (0, n, lastOutGain, g);
        lastOutGain = g;
    }

    if (get (pid::limiterOn) > 0.5f)
    {
        dsp::AudioBlock<float> block (buffer);
        dsp::ProcessContextReplacing<float> ctx (block);
        limiter.process (ctx);
    }
    for (int c = 0; c < 2; ++c)
        outLevel[c].store (buffer.getMagnitude (jmin (c, buffer.getNumChannels() - 1), 0, n), std::memory_order_relaxed);
}

void GrainFreezeAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr) return;

    auto* curves = xml->createNewChildElement ("CURVES");
    for (int n = 0; n < pid::numEnvelopes; ++n)
    {
        auto* e = curves->createNewChildElement ("ENV");
        e->setAttribute ("n", n);
        String pts;
        for (auto& p : mods[(size_t) n].getCurve())
            pts << String (p.x, 5) << "," << String (p.y, 5) << ";";
        e->setAttribute ("pts", pts);
    }
    auto* sq = xml->createNewChildElement ("SEQ");
    String lv;
    for (int i = 0; i < 32; ++i) lv << seq.getStep (i);
    sq->setAttribute ("levels", lv);

    copyXmlToBinary (*xml, dest);
}

void GrainFreezeAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;

    if (auto* curves = xml->getChildByName ("CURVES"))
    {
        for (auto* e : curves->getChildIterator())
        {
            const int n = e->getIntAttribute ("n", -1);
            if (! isPositiveAndBelow (n, pid::numEnvelopes)) continue;
            std::vector<Point<float>> pts;
            for (auto& tok : StringArray::fromTokens (e->getStringAttribute ("pts"), ";", ""))
            {
                auto xy = StringArray::fromTokens (tok, ",", "");
                if (xy.size() == 2) pts.push_back ({ jlimit (0.0f, 1.0f, xy[0].getFloatValue()),
                                                      jlimit (0.0f, 1.0f, xy[1].getFloatValue()) });
            }
            if (pts.size() >= 2) mods[(size_t) n].setCurve (pts);
        }
        xml->removeChildElement (curves, true);
    }
    if (auto* sq = xml->getChildByName ("SEQ"))
    {
        const auto lv = sq->getStringAttribute ("levels");
        for (int i = 0; i < jmin (32, lv.length()); ++i)
            seq.setStep (i, lv[i] - '0');
        xml->removeChildElement (sq, true);
    }

    apvts.replaceState (ValueTree::fromXml (*xml));
    curvesRevision.fetch_add (1);   // tells an open editor to re-read the curves
}

AudioProcessorEditor* GrainFreezeAudioProcessor::createEditor()
{
    return new GrainFreezeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainFreezeAudioProcessor();
}

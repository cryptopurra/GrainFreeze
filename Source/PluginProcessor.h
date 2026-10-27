#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "dsp/CaptureBuffer.h"
#include "dsp/GrainEngine.h"
#include "dsp/SpectralEngine.h"
#include "dsp/StepModulator.h"
#include "dsp/StepSequencer.h"
#include "dsp/Reducer.h"

// ---------------------------------------------------------------------------
// GrainFreezeAudioProcessor
//
// Signal flow per block:
//   1. input gain -> CaptureBuffer.writeBlock() (always)
//   2. read host transport (ppq, bpm, playing) for sync
//   3. evaluate the 6 StepModulators -> modulation matrix -> per-block targets
//   4. GrainEngine.process()  + SpectralEngine.process()
//   5. crossfade grain<->spectral by specMix
//   6. Reducer -> filters (HP/LP) -> step gate -> output gain -> limiter
//   7. dry/wet against the original input
// ---------------------------------------------------------------------------

class GrainFreezeAudioProcessor : public juce::AudioProcessor
{
public:
    GrainFreezeAudioProcessor();
    ~GrainFreezeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GrainFreeze"; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Exposed so the editor can draw / edit them.
    StepSequencer& sequencer()           noexcept { return seq; }
    StepModulator& modulator (int i)     noexcept { return mods[(size_t) i]; }

    // Current sequencer step, updated each block for the editor's playhead.
    std::atomic<int> currentStep { -1 };

private:
    // Applies all enabled modulators to the base parameter values for this block.
    void applyModulation (double ppqAtBlockStart);

    // Resolves a destination's final value (base + modulation) into the engines.
    struct BlockTargets
    {
        float grainSize, overlap, pitch, scanSpeed, jitter, spread;
        float specMix, specSmear, specHold, specBlur;
        float highpass, lowpass, robotize, reducer, volume;
    } tgt {};

    CaptureBuffer  capture;
    GrainEngine    grain;
    SpectralEngine spectral;
    StepSequencer  seq;
    Reducer        reducerFx;
    std::array<StepModulator, pid::numEnvelopes> mods;

    juce::AudioBuffer<float> grainBuf, specBuf, dryBuf;
    juce::dsp::StateVariableTPTFilter<float> hpFilter, lpFilter;
    juce::dsp::Limiter<float> limiter;

    juce::SmoothedValue<float> gateSmoothed;
    juce::SmoothedValue<float> inGainSm, outGainSm;

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeAudioProcessor)
};

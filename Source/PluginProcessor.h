#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "dsp/CaptureBuffer.h"
#include "dsp/GrainEngine.h"
#include "dsp/SpectralEngine.h"
#include "dsp/StepModulator.h"
#include "dsp/StepSequencer.h"
#include "dsp/Reducer.h"

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

    // --- voice sample loading (called from the editor / message thread) ---
    bool loadSampleFile (const juce::File& file);
    double getDetectedBpm() const noexcept { return detectedBpm.load(); }
    double getProjectBpm()  const noexcept { return projectBpm.load(); }
    juce::String getSampleName() const { return sampleName; }

    // Exposed for the editor.
    StepSequencer& sequencer()       noexcept { return seq; }
    StepModulator& modulator (int i) noexcept { return mods[(size_t) i]; }
    std::atomic<int> currentStep { -1 };

private:
    struct BlockTargets
    {
        float grainSize, overlap, pitchSemis, scanSpeed, volume, reducer;
        float specMix, specSmear, specHold, highpass, lowpass, robotize;
    } tgt {};

    void applyModulation (double ppq);

    CaptureBuffer  capture;
    GrainEngine    grain;
    SpectralEngine spectral;
    StepSequencer  seq;
    Reducer        reducerFx;
    std::array<StepModulator, pid::numEnvelopes> mods;

    juce::AudioFormatManager formatManager;
    std::atomic<double> detectedBpm { 120.0 };
    std::atomic<double> projectBpm  { 120.0 };
    juce::String sampleName;

    juce::AudioBuffer<float> grainBuf, specBuf, dryBuf;
    juce::dsp::StateVariableTPTFilter<float> hpFilter, lpFilter;
    juce::dsp::Limiter<float> limiter;

    juce::SmoothedValue<float> gateSmoothed, inGainSm, outGainSm, voiceGainSm;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeAudioProcessor)
};

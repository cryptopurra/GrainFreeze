#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/SequencerGrid.h"
#include "gui/CurveEditor.h"

// ---------------------------------------------------------------------------
// Minimal functional editor. The layout mirrors the mental model:
//   top row   : main knobs (grain size, overlap, pitch, speed, spec mix...)
//   centre    : big Freeze / Live toggle + source readout
//   left panel: the 6 envelope tabs (curve editor placeholder)
//   bottom    : the step-sequencer grid
// Visual design is intentionally plain here; it's the working scaffold you
// style later with the frontend-design pass.
// ---------------------------------------------------------------------------

class GrainFreezeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit GrainFreezeAudioProcessorEditor (GrainFreezeAudioProcessor&);
    ~GrainFreezeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttach = APVTS::SliderAttachment;
    using ButtonAttach = APVTS::ButtonAttachment;

    struct LabeledKnob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttach> attach;
    };

    LabeledKnob& addKnob (const juce::String& paramID, const juce::String& text);

    GrainFreezeAudioProcessor& proc;

    juce::OwnedArray<LabeledKnob> knobs;

    juce::ToggleButton freezeButton { "FREEZE" };
    std::unique_ptr<ButtonAttach> freezeAttach;

    // Step gate grid + one envelope curve editor (envelope selector + tabs
    // for all 6 come in the next pass; this wires the first envelope).
    SequencerGrid seqGrid { proc.sequencer() };
    CurveEditor    curve   { proc.modulator (0) };
    juce::Label    seqLabel, curveLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeAudioProcessorEditor)
};

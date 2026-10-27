#include "PluginEditor.h"

using namespace juce;

GrainFreezeAudioProcessorEditor::GrainFreezeAudioProcessorEditor (GrainFreezeAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    // Main knobs across the top. (IDs come from Parameters.h.)
    addKnob (pid::grainSize,    "Grain Size");
    addKnob (pid::grainOverlap, "Overlap");
    addKnob (pid::grainPitch,   "Pitch");
    addKnob (pid::scanSpeed,    "Speed");
    addKnob (pid::specMix,      "Spec Mix");
    addKnob (pid::specSmear,    "Smear");
    addKnob (pid::specHold,     "Hold");
    addKnob (pid::reducer,      "Reducer");
    addKnob (pid::outputGain,   "Output");
    addKnob (pid::dryWet,       "Dry/Wet");

    freezeButton.setClickingTogglesState (true);
    addAndMakeVisible (freezeButton);
    freezeAttach = std::make_unique<ButtonAttach> (proc.apvts, pid::freeze, freezeButton);

    seqLabel.setText ("Step Gate (click: level  /  right-click: clear)", dontSendNotification);
    seqLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.5f));
    addAndMakeVisible (seqLabel);
    addAndMakeVisible (seqGrid);

    curveLabel.setText ("Envelope 1 (click: add point  /  drag: move  /  dbl-click: remove)", dontSendNotification);
    curveLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.5f));
    addAndMakeVisible (curveLabel);
    addAndMakeVisible (curve);

    // Drive the grid playhead from the processor's sequencer at ~30 Hz.
    startTimerHz (30);

    setSize (900, 560);
}

GrainFreezeAudioProcessorEditor::LabeledKnob&
GrainFreezeAudioProcessorEditor::addKnob (const String& paramID, const String& text)
{
    auto* k = new LabeledKnob();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 72, 16);
    addAndMakeVisible (k->slider);

    k->label.setText (text, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    addAndMakeVisible (k->label);

    k->attach = std::make_unique<SliderAttach> (proc.apvts, paramID, k->slider);
    knobs.add (k);
    return *k;
}

void GrainFreezeAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff1b1d22));
    g.setColour (Colour (0xff2a2d35));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.f), 10.f);

    g.setColour (Colours::white.withAlpha (0.85f));
    g.setFont (18.0f);
    g.drawText ("GrainFreeze", 20, 12, 300, 24, Justification::left);

    g.setColour (Colours::white.withAlpha (0.4f));
    g.setFont (12.0f);
    g.drawText (proc.sequencer().isEnabled() ? "SEQ ON" : "SEQ OFF",
                getWidth() - 120, 14, 100, 20, Justification::right);
}

void GrainFreezeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (40); // title strip

    // Big freeze toggle in the centre-left.
    freezeButton.setBounds (area.removeFromTop (60).withSizeKeepingCentre (160, 48));

    // Knob row(s): wrap into a grid.
    const int kw = 78, kh = 96, gap = 6;
    int x = area.getX(), y = area.getY() + 10;
    for (auto* k : knobs)
    {
        k->label.setBounds (x, y, kw, 16);
        k->slider.setBounds (x, y + 16, kw, kh - 16);
        x += kw + gap;
        if (x + kw > area.getRight()) { x = area.getX(); y += kh + gap; }
    }

    // Bottom half: envelope curve editor on the left, step grid below.
    auto bottom = getLocalBounds().reduced (20).removeFromBottom (230);

    auto curveArea = bottom.removeFromTop (130);
    curveLabel.setBounds (curveArea.removeFromTop (18));
    curve.setBounds (curveArea);

    bottom.removeFromTop (8);
    seqLabel.setBounds (bottom.removeFromTop (18));
    seqGrid.setBounds (bottom.removeFromTop (70));
}

void GrainFreezeAudioProcessorEditor::timerCallback()
{
    // Reflect the live playhead on the grid from the processor's atomic step.
    seqGrid.setPlayStep (proc.currentStep.load (std::memory_order_relaxed));
}

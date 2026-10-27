#pragma once
#include <JuceHeader.h>
#include "GFLookAndFeel.h"

// ---------------------------------------------------------------------------
// ValueBox  --  dark inset numeric readout bound to a parameter. Drag up/down
// to change (shift = fine), mouse-wheel to nudge, double-click to type a value.
// ---------------------------------------------------------------------------
class ValueBox : public juce::Component
{
public:
    explicit ValueBox (juce::RangedAudioParameter& p)
        : param (p),
          att (p, [this] (float) { repaint(); })
    {
        att.sendInitialUpdate();
        addChildComponent (editor);
        editor.setJustification (juce::Justification::centred);
        editor.setSelectAllWhenFocused (true);
        editor.onReturnKey = [this] { commit(); };
        editor.onEscapeKey = [this] { editor.setVisible (false); };
        editor.onFocusLost = [this] { editor.setVisible (false); };
    }

    void setTextColour (juce::Colour c) { textCol = c; repaint(); }

    void resized() override { editor.setBounds (getLocalBounds().reduced (1)); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (gf::inset());
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff30343d));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (isEnabled() ? textCol : textCol.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (juce::jmin (14.0f, r.getHeight() * 0.62f), juce::Font::bold)));
        g.drawText (param.getText (param.getValue(), 32), r, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        startNorm = param.getValue();
        att.beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float speed = e.mods.isShiftDown() ? 800.0f : 200.0f;
        const float v = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / speed);
        att.setValueAsPartOfGesture (param.convertFrom0to1 (v));
    }
    void mouseUp (const juce::MouseEvent&) override { att.endGesture(); }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float v = juce::jlimit (0.0f, 1.0f, param.getValue() + w.deltaY * 0.05f);
        att.setValueAsCompleteGesture (param.convertFrom0to1 (v));
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        editor.setText (param.getText (param.getValue(), 32), false);
        editor.setVisible (true);
        editor.grabKeyboardFocus();
    }

private:
    void commit()
    {
        const float norm = juce::jlimit (0.0f, 1.0f, param.getValueForText (editor.getText()));
        editor.setVisible (false);
        att.setValueAsCompleteGesture (param.convertFrom0to1 (norm));
    }

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment att;
    juce::TextEditor editor;
    juce::Colour textCol = gf::gold();
    float startNorm = 0.0f;
};

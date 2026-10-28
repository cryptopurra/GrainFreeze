#pragma once
#include <JuceHeader.h>
#include "GFLookAndFeel.h"

// ---------------------------------------------------------------------------
// CycleButton  --  a choice parameter shown as a dark box. Click opens a list
// to pick the option; the mouse wheel over it steps through the options.
// ---------------------------------------------------------------------------
class CycleButton : public juce::Component
{
public:
    CycleButton (juce::RangedAudioParameter& p, juce::StringArray choiceNames)
        : names (std::move (choiceNames)),
          att (p, [this] (float v)
          {
              idx = juce::jlimit (0, juce::jmax (0, names.size() - 1), (int) std::lround (v));
              repaint();
              if (onChanged) onChanged (idx);
          })
    {
        att.sendInitialUpdate();
    }

    std::function<void (int)> onChanged;
    int getIndex() const noexcept { return idx; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (isMouseOver() ? juce::Colour (0xff15171b) : gf::inset());
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff30343d));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (gf::gold());
        g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        g.drawText (names[idx], r.withTrimmedRight (12.0f), juce::Justification::centred);

        juce::Path tri;                                   // small drop-down arrow
        const float cx = r.getRight() - 9.0f, cy = r.getCentreY();
        tri.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour (gf::goldDim());
        g.fillPath (tri);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        juce::PopupMenu m;
        for (int i = 0; i < names.size(); ++i)
            m.addItem (i + 1, names[i], true, i == idx);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
            [safe = juce::Component::SafePointer<CycleButton> (this)] (int id)
            {
                if (safe != nullptr && id > 0) safe->set (id - 1);
            });
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY == 0.0f) return;
        set (juce::jlimit (0, names.size() - 1, idx + (w.deltaY > 0 ? -1 : 1)));   // wheel up = previous option
    }

private:
    void set (int i) { if (i != idx) att.setValueAsCompleteGesture ((float) i); }

    juce::StringArray names;
    juce::ParameterAttachment att;
    int idx = 0;
};

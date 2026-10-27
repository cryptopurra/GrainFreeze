#pragma once
#include <JuceHeader.h>
#include "GFLookAndFeel.h"

// ---------------------------------------------------------------------------
// CycleButton  --  click to step through a choice parameter (right-click = back).
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
        g.drawText (names[idx], r, juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()) || names.isEmpty()) return;
        const int d = e.mods.isPopupMenu() ? -1 : 1;
        att.setValueAsCompleteGesture ((float) ((idx + d + names.size()) % names.size()));
    }

private:
    juce::StringArray names;
    juce::ParameterAttachment att;
    int idx = 0;
};

#pragma once
#include <JuceHeader.h>

// FREEZE: left mouse = frozen only while the button is held;
//         right mouse = latches on / off.
class FreezeButton : public juce::ToggleButton
{
public:
    explicit FreezeButton (juce::RangedAudioParameter& p)
        : juce::ToggleButton ("FREEZE"),
          att (p, [this] (float v)
          {
              if (! held) latched = v > 0.5f;
              setToggleState (v > 0.5f || held, juce::dontSendNotification);
          })
    {
        setClickingTogglesState (false);
        setTooltip ("Left click: frozen while held.  Right click: latch on / off.");
        att.sendInitialUpdate();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { latched = ! latched; downWasLeft = false; }
        else                      { held = true;          downWasLeft = true;  }
        push();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (downWasLeft) { held = false; downWasLeft = false; push(); }
    }

    bool hitTest (int x, int y) override       // round button
    {
        const auto c = getLocalBounds().getCentre().toFloat();
        return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) juce::jmin (getWidth(), getHeight()) * 0.5f;
    }

private:
    void push()
    {
        const bool on = held || latched;
        att.setValueAsCompleteGesture (on ? 1.0f : 0.0f);
        setToggleState (on, juce::dontSendNotification);
    }

    juce::ParameterAttachment att;
    bool held = false, latched = false, downWasLeft = false;
};

#pragma once
#include <JuceHeader.h>
#include "GFLookAndFeel.h"

// Preset selector:  [<]  name  [>]   click the name for the list, wheel to step.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (juce::StringArray presetNames) : names (std::move (presetNames)) {}

    std::function<void (int)> onSelect;
    void setCurrent (int i) { if (i != current) { current = i; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (gf::inset());
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff30343d));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        g.setColour (gf::gold());
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (names[current], r.withTrimmedLeft (24.0f).withTrimmedRight (24.0f), juce::Justification::centred);

        auto arrow = [&] (float cx, bool left)
        {
            juce::Path p;
            const float cy = r.getCentreY();
            if (left) p.addTriangle (cx + 3.0f, cy - 5.0f, cx + 3.0f, cy + 5.0f, cx - 3.0f, cy);
            else      p.addTriangle (cx - 3.0f, cy - 5.0f, cx - 3.0f, cy + 5.0f, cx + 3.0f, cy);
            g.setColour (gf::goldDim());
            g.fillPath (p);
        };
        arrow (r.getX() + 12.0f, true);
        arrow (r.getRight() - 12.0f, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < 24)                  { step (-1); return; }
        if (e.x > getWidth() - 24)     { step (+1); return; }

        juce::PopupMenu m;
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == current);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
            [safe = juce::Component::SafePointer<PresetBar> (this)] (int id)
            {
                if (safe != nullptr && id > 0 && safe->onSelect) safe->onSelect (id - 1);
            });
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f) step (w.deltaY > 0 ? -1 : 1);
    }

private:
    void step (int d)
    {
        const int n = juce::jlimit (0, names.size() - 1, current + d);
        if (n != current && onSelect) onSelect (n);
    }
    juce::StringArray names;
    int current = 0;
};

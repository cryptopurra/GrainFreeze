#pragma once
#include <JuceHeader.h>
#include "GFLookAndFeel.h"

// ---------------------------------------------------------------------------
// VoicePanel  --  drop zone + classic waveform view for the voice sample.
//
// Shows the min/max overview, a beat grid at the detected tempo, the playhead,
// and a status line (file name, detected BPM -> project BPM, stretch ratio).
// Drop an audio file on it, or click it (empty) / double-click it (loaded) to browse.
// (Original component.)
// ---------------------------------------------------------------------------

class VoicePanel : public juce::Component,
                   public juce::FileDragAndDropTarget
{
public:
    std::function<void (juce::File)> onFileChosen;

    void setPeaks (std::vector<float> p, double seconds)
    {
        peaks = std::move (p);
        duration = seconds;
        peakMax = 0.0f;
        for (auto v : peaks) peakMax = juce::jmax (peakMax, std::abs (v));
        repaint();
    }

    void setInfo (const juce::String& name, double detectedBpm, double projectBpm, bool synced, bool frozenNow)
    {
        if (name != loadedName || detectedBpm != detBpm || projectBpm != projBpm || synced != sync || frozenNow != frozen)
        {
            loadedName = name; detBpm = detectedBpm; projBpm = projectBpm; sync = synced; frozen = frozenNow;
            repaint();
        }
    }

    void setPlayhead (float norm)
    {
        if (std::abs (norm - playNorm) > 0.0005f) { playNorm = norm; repaint(); }
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff")
             || f.endsWithIgnoreCase (".mp3") || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".ogg"))
                return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { hover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        hover = false; repaint();
        if (files.size() > 0 && onFileChosen) onFileChosen (juce::File (files[0]));
    }

    void browse()
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a voice / sample", juce::File{}, "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile() && onFileChosen) onFileChosen (f);
            });
    }

    void mouseDown (const juce::MouseEvent&) override       { if (peaks.empty()) browse(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (! peaks.empty()) browse(); }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.setColour (hover ? juce::Colour (0xff17261a) : juce::Colour (0xff0b0c0f));
        g.fillRoundedRectangle (full, 5.0f);
        g.setColour (hover ? juce::Colour (0xff3fbf6a) : juce::Colour (0xff2c3038));
        g.drawRoundedRectangle (full.reduced (0.5f), 5.0f, hover ? 2.0f : 1.0f);

        auto area = full.reduced (6.0f, 18.0f);
        const float midY = area.getCentreY();

        // centre line
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());

        if (peaks.empty())
        {
            g.setColour (gf::textDim());
            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.drawText ("Drop an audio file here  (or click to browse)", full, juce::Justification::centred);
            return;
        }

        // beat grid at the detected tempo
        if (detBpm > 0.0 && duration > 0.0)
        {
            const double beat = 60.0 / detBpm;
            int i = 0;
            for (double t = 0.0; t < duration; t += beat, ++i)
            {
                const float x = area.getX() + (float) (t / duration) * area.getWidth();
                g.setColour (juce::Colours::white.withAlpha ((i % 4) == 0 ? 0.20f : 0.07f));
                g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            }
        }

        // waveform
        const int cols = (int) peaks.size() / 2;
        const float scale = peakMax > 0.0001f ? 0.92f / peakMax : 1.0f;
        const float halfH = area.getHeight() * 0.5f;
        const int w = juce::jmax (1, (int) area.getWidth());
        juce::Path top, bottom; juce::ignoreUnused (bottom);
        for (int px = 0; px < w; ++px)
        {
            const int c0 = px * cols / w, c1 = juce::jmax (c0 + 1, (px + 1) * cols / w);
            float lo = 0.0f, hi = 0.0f;
            for (int c = c0; c < c1 && c < cols; ++c)
            {
                lo = juce::jmin (lo, peaks[(size_t) c * 2]);
                hi = juce::jmax (hi, peaks[(size_t) c * 2 + 1]);
            }
            const float x = area.getX() + (float) px;
            if (px == 0) { top.startNewSubPath (x, midY - hi * scale * halfH); bottom.startNewSubPath (x, midY - lo * scale * halfH); }
            else         { top.lineTo (x, midY - hi * scale * halfH);          bottom.lineTo (x, midY - lo * scale * halfH); }
        }
        juce::Path shape (top);
        {
            // close the shape by walking the bottom edge back
            for (int px = w - 1; px >= 0; --px)
            {
                const int c0 = px * cols / w, c1 = juce::jmax (c0 + 1, (px + 1) * cols / w);
                float lo = 0.0f;
                for (int c = c0; c < c1 && c < cols; ++c) lo = juce::jmin (lo, peaks[(size_t) c * 2]);
                shape.lineTo (area.getX() + (float) px, midY - lo * scale * halfH);
            }
            shape.closeSubPath();
        }
        juce::ColourGradient grad (juce::Colour (0xff62e0d2), 0, area.getY(), juce::Colour (0xff2a8f9a), 0, area.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (shape);

        // playhead
        if (playNorm >= 0.0f)
        {
            const float x = area.getX() + playNorm * area.getWidth();
            g.setColour (frozen ? gf::accent() : juce::Colours::white);
            g.drawVerticalLine ((int) x, area.getY() - 6.0f, area.getBottom() + 6.0f);
            juce::Path tri;
            tri.addTriangle (x - 5.0f, area.getY() - 6.0f, x + 5.0f, area.getY() - 6.0f, x, area.getY() + 1.0f);
            g.fillPath (tri);
        }

        // status line
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.setColour (juce::Colour (0xffd8dde6));
        g.drawText (loadedName, full.withTrimmedLeft (10.0f).withHeight (18.0f), juce::Justification::centredLeft);
        g.setColour (gf::textDim());
        const double ratio = detBpm > 0.0 ? projBpm / detBpm : 1.0;
        g.drawText (juce::String (detBpm, 1) + " BPM  ->  " + juce::String (projBpm, 1) + " BPM  (x"
                    + juce::String (ratio, 3) + (sync ? ", synced)" : ", manual stretch)"),
                    full.withTrimmedRight (10.0f).withHeight (18.0f), juce::Justification::centredRight);
    }

private:
    std::vector<float> peaks;
    double duration = 0.0, detBpm = 0.0, projBpm = 0.0;
    float peakMax = 0.0f, playNorm = -1.0f;
    juce::String loadedName;
    bool hover = false, sync = true, frozen = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

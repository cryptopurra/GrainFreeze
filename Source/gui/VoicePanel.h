#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// VoicePanel  --  drop zone for an audio sample ("voice") plus an info line.
//
// Accepts a dragged audio file or a click-to-browse. Reports the chosen file
// through onFileChosen. Shows the loaded name and the detected / project BPM
// (fed by the editor via setInfo).
// ---------------------------------------------------------------------------

class VoicePanel : public juce::Component,
                   public juce::FileDragAndDropTarget
{
public:
    std::function<void (juce::File)> onFileChosen;

    void setInfo (const juce::String& name, double detectedBpm, double projectBpm)
    {
        loadedName = name;
        detBpm = detectedBpm;
        projBpm = projectBpm;
        repaint();
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif")
             || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".mp3")
             || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".ogg"))
                return true;
        return false;
    }

    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { hover = false; repaint(); }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        hover = false; repaint();
        if (files.size() > 0 && onFileChosen)
            onFileChosen (juce::File (files[0]));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a voice / sample",
                      juce::File{}, "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile() && onFileChosen) onFileChosen (f);
            });
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (hover ? 0xff294a29 : 0xff20232b));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xff3a3f4a));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);

        g.setColour (juce::Colour (0xff8b93a1));
        g.setFont (12.0f);
        if (loadedName.isEmpty())
            g.drawText ("Drop a voice / sample here  (or click to browse)",
                        r, juce::Justification::centred);
        else
        {
            g.setColour (juce::Colour (0xffd8dde6));
            g.drawText (loadedName, r.removeFromTop (r.getHeight() * 0.5f),
                        juce::Justification::centred);
            g.setColour (juce::Colour (0xff8b93a1));
            g.drawText ("sample " + juce::String (detBpm, 1) + " BPM  ->  project "
                        + juce::String (projBpm, 1) + " BPM",
                        r, juce::Justification::centred);
        }
    }

private:
    juce::String loadedName;
    double detBpm = 0.0, projBpm = 0.0;
    bool hover = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

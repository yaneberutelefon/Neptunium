#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace nep
{
    const juce::Colour violet  { 0xff9b5cff };
    const juce::Colour light   { 0xffc9a8ff };
    const juce::Colour ink     { 0xfff4f1fa };
    const juce::Colour dim     { 0xff8d86a3 };
    const juce::Colour line    { 0xff2a2340 };

    inline juce::Font serif (float h, int style = juce::Font::plain)
    {
        return juce::Font ("Georgia", h, style);
    }
}

//==============================================================================
struct NeptuniumLook : public juce::LookAndFeel_V4
{
    NeptuniumLook()
    {
        setColour (juce::Slider::textBoxTextColourId, nep::light);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::textColourOffId, nep::ink);
        setColour (juce::TextButton::textColourOnId, nep::ink);
        setColour (juce::ToggleButton::textColourId, nep::ink);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return nep::serif (18.0f); }
    juce::Font getLabelFont (juce::Label&) override { return nep::serif (16.0f); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (10.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = start + pos * (end - start);

        // soft violet halo
        g.setGradientFill (juce::ColourGradient (nep::violet.withAlpha (0.22f), c.x, c.y,
                                                 juce::Colours::transparentBlack, c.x, c.y - r * 1.5f, true));
        g.fillEllipse (c.x - r * 1.5f, c.y - r * 1.5f, r * 3.0f, r * 3.0f);

        // dark body
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1436), c.x, c.y - r,
                                                 juce::Colour (0xff09060f), c.x, c.y + r, false));
        g.fillEllipse (c.x - r * 0.74f, c.y - r * 0.74f, r * 1.48f, r * 1.48f);
        g.setColour (nep::line);
        g.drawEllipse (c.x - r * 0.74f, c.y - r * 0.74f, r * 1.48f, r * 1.48f, 1.0f);

        // track
        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
        g.setColour (nep::line);
        g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value
        juce::Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.0f, start, angle, true);
        g.setGradientFill (juce::ColourGradient (nep::violet, c.x - r, c.y, nep::light, c.x + r, c.y, false));
        g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // pointer: a small star
        const auto p = c.getPointOnCircumference (r * 0.5f, angle);
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));
        g.setColour (nep::light.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (p));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        if (b.getProperties().contains ("primary"))
        {
            g.setColour (down ? nep::violet.darker (0.2f) : (highlighted ? nep::light : nep::violet));
            g.fillRoundedRectangle (r, 2.0f);
        }
        else
        {
            if (highlighted || down)
            {
                g.setColour (nep::violet.withAlpha (down ? 0.3f : 0.16f));
                g.fillRoundedRectangle (r, 2.0f);
            }
            g.setColour (nep::violet);
            g.drawRoundedRectangle (r, 2.0f, 1.2f);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float d = 8.0f;
        const float cx = r.getX() + d + 1.0f, cy = r.getCentreY();
        juce::Path p;
        p.addQuadrilateral (cx, cy - d, cx + d, cy, cx, cy + d, cx - d, cy);
        if (b.getToggleState())
        {
            g.setColour (nep::violet);
            g.fillPath (p);
        }
        g.setColour (highlighted ? nep::light : nep::violet);
        g.strokePath (p, juce::PathStrokeType (1.2f));
        g.setColour (nep::ink);
        g.setFont (nep::serif (17.0f));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (28.0f).toNearestInt(), juce::Justification::centredLeft);
    }
};

//==============================================================================
class DragChip : public juce::Component
{
public:
    std::function<juce::File()> getFile;

    void paint (juce::Graphics& g) override
    {
        const bool ok = getFile && getFile().existsAsFile();
        auto b = getLocalBounds().toFloat().reduced (1.0f);

        juce::Path rect, dashed;
        rect.addRoundedRectangle (b, 2.0f);
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, rect, dashes, 2);

        g.setColour (nep::violet.withAlpha (ok ? (over ? 1.0f : 0.85f) : 0.3f));
        g.fillPath (dashed);
        if (ok && over)
        {
            g.setColour (nep::violet.withAlpha (0.14f));
            g.fillRoundedRectangle (b, 2.0f);
        }
        g.setColour (nep::ink.withAlpha (ok ? 0.95f : 0.35f));
        g.setFont (nep::serif (18.0f));
        g.drawText ("Drag to playlist", getLocalBounds(), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseDown  (const juce::MouseEvent&) override { dragging = false; }
    void mouseUp    (const juce::MouseEvent&) override { dragging = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6 || ! getFile)
            return;
        const auto f = getFile();
        if (f.existsAsFile())
        {
            dragging = true;
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
        }
    }

private:
    bool over = false, dragging = false;
};

//==============================================================================
class NeptuniumEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit NeptuniumEditor (NeptuniumProcessor&);
    ~NeptuniumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Star { float x, y, r, phase, speed; bool violet; };

    void timerCallback() override;
    void load (const juce::File&);
    void browse();
    void drawZone (juce::Graphics&);

    NeptuniumLook look;
    NeptuniumProcessor& proc;
    NeptuniumProcessor::Info info;
    int lastVersion = -1;
    bool dragOver = false;
    juce::String message;

    juce::Rectangle<float> zone;
    juce::Image bg;
    std::vector<Star> twinkle;

    juce::Slider lengthS, densityS, toneS, wetS;
    juce::ToggleButton onlyBtn { "Reverb only before the line" }, dryBtn { "Include original vocal" };
    juce::TextButton previewBtn { "Preview" }, folderBtn { "Folder" };
    DragChip chip;
    std::unique_ptr<juce::FileChooser> chooser;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aLength, aDensity, aTone, aWet;
    std::unique_ptr<BA> aOnly, aDry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeptuniumEditor)
};

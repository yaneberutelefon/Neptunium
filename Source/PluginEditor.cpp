#include "PluginEditor.h"

NeptuniumEditor::NeptuniumEditor (NeptuniumProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);

    auto setup = [this] (juce::Slider& s, const juce::String& suffix, int decimals)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (decimals);
        addAndMakeVisible (s);
    };
    setup (lengthS,  " s",  1);
    setup (densityS, "",    1);
    setup (toneS,    " Hz", 0);
    setup (wetS,     " dB", 1);

    aLength  = std::make_unique<SA> (proc.apvts, "length",  lengthS);
    aDensity = std::make_unique<SA> (proc.apvts, "density", densityS);
    aTone    = std::make_unique<SA> (proc.apvts, "tone",    toneS);
    aWet     = std::make_unique<SA> (proc.apvts, "wet",     wetS);
    aOnly    = std::make_unique<BA> (proc.apvts, "only", onlyBtn);
    aDry     = std::make_unique<BA> (proc.apvts, "dry",  dryBtn);

    addAndMakeVisible (onlyBtn);
    addAndMakeVisible (dryBtn);

    previewBtn.getProperties().set ("primary", true);
    previewBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0a0612));
    previewBtn.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff0a0612));
    previewBtn.onClick = [this] { proc.setPreview (! proc.isPreviewing()); };
    addAndMakeVisible (previewBtn);

    folderBtn.onClick = [this]
    {
        if (info.outFile.existsAsFile())
            info.outFile.revealToUser();
        else
        {
            auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Neptunium");
            d.createDirectory();
            d.startAsProcess();
        }
    };
    addAndMakeVisible (folderBtn);

    chip.getFile = [this] { return info.outFile; };
    addAndMakeVisible (chip);

    juce::Random rnd (42);
    for (int i = 0; i < 46; ++i)
        twinkle.push_back ({ rnd.nextFloat(), rnd.nextFloat(), 0.7f + rnd.nextFloat() * 1.3f,
                             rnd.nextFloat() * 6.28f, 0.6f + rnd.nextFloat() * 1.6f, rnd.nextFloat() < 0.3f });

    info = proc.getInfo();
    setSize (760, 520);
    startTimerHz (30);
}

NeptuniumEditor::~NeptuniumEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void NeptuniumEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    zone = { 40.0f, 100.0f, (float) (w - 80), 150.0f };

    const int colW = (w - 80) / 4;
    juce::Slider* knobs[] = { &lengthS, &densityS, &toneS, &wetS };
    for (int i = 0; i < 4; ++i)
        knobs[i]->setBounds (40 + i * colW + 10, 292, colW - 20, 132);

    onlyBtn.setBounds (40, 446, 240, 26);
    dryBtn.setBounds  (40, 476, 240, 26);
    chip.setBounds (w - 40 - 200, 448, 200, 48);
    previewBtn.setBounds (w - 40 - 200 - 12 - 110, 448, 110, 48);
    folderBtn.setBounds (previewBtn.getX() - 12 - 90, 448, 90, 48);

    // static background: deep violet night, dust of stars, a thin ring behind the title
    bg = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (bg);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1036), w * 0.5f, 0.0f,
                                             juce::Colour (0xff050408), w * 0.5f, (float) h, true));
    g.fillAll();

    juce::Random rnd (7);
    for (int i = 0; i < 160; ++i)
    {
        const float sx = rnd.nextFloat() * w, sy = rnd.nextFloat() * h, sr = 0.4f + rnd.nextFloat() * 0.9f;
        g.setColour ((rnd.nextFloat() < 0.2f ? nep::light : juce::Colours::white).withAlpha (0.15f + rnd.nextFloat() * 0.4f));
        g.fillEllipse (sx, sy, sr, sr);
    }

    g.setGradientFill ([&] {
        juce::ColourGradient cg (juce::Colours::transparentBlack, w * 0.5f - 250.0f, 0.0f,
                                 juce::Colours::transparentBlack, w * 0.5f + 250.0f, 0.0f, false);
        cg.addColour (0.5, nep::violet.withAlpha (0.55f));
        return cg; }());
    g.drawEllipse (w * 0.5f - 250.0f, 44.0f, 500.0f, 44.0f, 1.0f);
}

//==============================================================================
void NeptuniumEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (bg, 0, 0);

    const float t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
    for (auto& s : twinkle)
    {
        const float a = 0.15f + 0.85f * std::pow (0.5f + 0.5f * std::sin (s.phase + t * s.speed), 2.0f);
        g.setColour ((s.violet ? nep::light : juce::Colours::white).withAlpha (a));
        const float sx = s.x * getWidth(), sy = s.y * getHeight();
        g.fillEllipse (sx, sy, s.r, s.r);
        if (s.r > 1.5f)
        {
            g.setColour ((s.violet ? nep::light : juce::Colours::white).withAlpha (a * 0.4f));
            g.drawLine (sx - 4.0f, sy + s.r * 0.5f, sx + 4.0f + s.r, sy + s.r * 0.5f, 0.6f);
            g.drawLine (sx + s.r * 0.5f, sy - 4.0f, sx + s.r * 0.5f, sy + 4.0f + s.r, 0.6f);
        }
    }

    // logotype
    auto tf = nep::serif (40.0f);
    tf.setExtraKerningFactor (0.34f);
    g.setFont (tf);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0.0f, 24.0f, juce::Colour (0xffb68cff), 0.0f, 68.0f, false));
    g.drawText ("NEPTUNIUM", juce::Rectangle<int> (0, 22, getWidth(), 52), juce::Justification::centred, false);
    g.setColour (nep::dim);
    g.setFont (nep::serif (15.0f, juce::Font::italic));
    g.drawText ("reverse reverb", juce::Rectangle<int> (0, 76, getWidth(), 20), juce::Justification::centred, false);

    drawZone (g);

    // knob captions
    const int colW = (getWidth() - 80) / 4;
    const char* names[] = { "Length", "Density", "Tone", "Wet" };
    g.setFont (nep::serif (17.0f));
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (nep::ink.withAlpha (0.85f));
        g.drawText (names[i], 40 + i * colW, 268, colW, 22, juce::Justification::centred, false);
    }
}

void NeptuniumEditor::drawZone (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0d0919).withAlpha (0.72f));
    g.fillRoundedRectangle (zone, 3.0f);

    const bool hot = dragOver;
    if (hot)
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (nep::violet.withAlpha (0.07f));
            g.drawRoundedRectangle (zone.expanded ((float) i * 2.0f), 3.0f + i, 2.0f);
        }
    g.setColour (hot ? nep::violet : nep::line);
    g.drawRoundedRectangle (zone, 3.0f, 1.2f);

    const bool ready = info.hasFile && ! info.peaks.empty();

    if (! ready)
    {
        g.setColour (nep::ink);
        g.setFont (nep::serif (27.0f));
        g.drawText (info.hasFile ? "Rendering" : "Drop a vocal line here",
                    zone.withTrimmedBottom (22.0f).toNearestInt(), juce::Justification::centred, false);
        g.setColour (message.isNotEmpty() ? nep::light : nep::dim);
        g.setFont (nep::serif (15.0f, juce::Font::italic));
        g.drawText (message.isNotEmpty() ? message : "wav, mp3, flac, ogg, aiff  -  or click to browse",
                    zone.withTrimmedTop (84.0f).toNearestInt(), juce::Justification::centred, false);
        return;
    }

    auto area = zone.reduced (22.0f, 34.0f);
    const float mid = area.getCentreY();
    const int n = (int) info.peaks.size();

    juce::ColourGradient cg (nep::violet.darker (0.3f), area.getX(), 0.0f, juce::Colours::white, area.getRight(), 0.0f, false);
    cg.addColour (juce::jlimit (0.01, 0.99, (double) info.offsetFrac), nep::light);
    g.setGradientFill (cg);
    for (int i = 0; i < n; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) (n - 1);
        const float hh = info.peaks[(size_t) i] * area.getHeight() * 0.5f;
        g.fillRect (x, mid - hh, 1.5f, juce::jmax (1.0f, hh * 2.0f));
    }

    const float mx = area.getX() + area.getWidth() * info.offsetFrac;
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawLine (mx, area.getY() - 12.0f, mx, area.getBottom() + 12.0f, 1.0f);
    g.setFont (nep::serif (14.0f, juce::Font::italic));
    g.setColour (nep::light);
    g.drawText ("line starts at " + juce::String (info.offsetSeconds, 2) + " s",
                juce::Rectangle<float> (mx + 6.0f, zone.getY() + 8.0f, 200.0f, 18.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
    g.setColour (nep::dim);
    g.drawText (info.name, juce::Rectangle<float> (zone.getX() + 16.0f, zone.getBottom() - 26.0f, zone.getWidth() - 32.0f, 20.0f).toNearestInt(),
                juce::Justification::centredLeft, false);

    if (proc.isBusy())
    {
        g.setColour (juce::Colour (0xff0d0919).withAlpha (0.6f));
        g.fillRoundedRectangle (zone, 3.0f);
        g.setColour (nep::ink);
        g.setFont (nep::serif (22.0f, juce::Font::italic));
        g.drawText ("Rendering", zone.toNearestInt(), juce::Justification::centred, false);
    }
}

//==============================================================================
void NeptuniumEditor::timerCallback()
{
    const int v = proc.getVersion();
    if (v != lastVersion)
    {
        lastVersion = v;
        info = proc.getInfo();
    }
    previewBtn.setButtonText (proc.isPreviewing() ? "Stop" : "Preview");
    repaint();
}

bool NeptuniumEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg" || ext == ".aif" || ext == ".aiff")
            return true;
    }
    return false;
}

void NeptuniumEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    if (files.size() > 0)
        load (juce::File (files[0]));
}

void NeptuniumEditor::load (const juce::File& f)
{
    if (proc.loadFile (f))
        message = {};
    else
        message = "Could not read that file. Use wav, mp3, flac, ogg or aiff.";
}

void NeptuniumEditor::mouseUp (const juce::MouseEvent& e)
{
    if (zone.contains (e.position) && e.getDistanceFromDragStart() < 4)
        browse();
}

void NeptuniumEditor::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a vocal line", juce::File(), "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff");
    juce::Component::SafePointer<NeptuniumEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile())
                                  safe->load (f);
                          });
}

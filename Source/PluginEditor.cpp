#include "PluginEditor.h"

namespace colours
{
    const juce::Colour bg       { 0xff0e0b14 };
    const juce::Colour panel    { 0xff1a1424 };
    const juce::Colour accent   { 0xffb04dff };
    const juce::Colour accent2  { 0xff36e0ff };
    const juce::Colour text     { 0xffe9e3f5 };
    const juce::Colour dim      { 0xff7d7290 };
}

static juce::String noteName (int midi)
{
    if (midi < 0) return "--";
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (n[midi % 12]) + juce::String (midi / 12 - 1);
}

//==============================================================================
ZavLookAndFeel::ZavLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::panel);
    setColour (juce::ComboBox::outlineColourId, colours::accent.withAlpha (0.5f));
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::textColourId, colours::text);
}

void ZavLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6);
    auto r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto c = bounds.getCentre();
    auto angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (colours::panel.brighter (0.15f));
    g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, r * 0.9f, r * 0.9f, 0, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, r * 0.9f, r * 0.9f, 0, startAngle, angle, true);
    g.setColour (colours::dim.withAlpha (0.35f));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (colours::accent, c.x - r, c.y, colours::accent2, c.x + r, c.y, false));
    g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -r * 0.68f, 3.0f, r * 0.35f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
ZavTuneEditor::ZavTuneEditor (ZavTuneProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (retune, "retune", "RETUNE SPEED");
    setupKnob (amount, "amount", "AMOUNT");
    setupKnob (humanize, "humanize", "HUMANIZE");
    setupKnob (transpose, "transpose", "TRANSPOSE");
    setupKnob (mix, "mix", "MIX");

    auto setupBox = [this] (juce::ComboBox& box, juce::Label& label, const juce::String& id, const juce::String& name)
    {
        auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id));
        box.addItemList (param->choices, 1);
        addAndMakeVisible (box);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, colours::dim);
        addAndMakeVisible (label);
    };
    setupBox (keyBox, keyLabel, "key", "KEY");
    setupBox (scaleBox, scaleLabel, "scale", "SCALE");
    keyAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);

    lowLatButton.setClickingTogglesState (true);
    lowLatButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    lowLatButton.setColour (juce::TextButton::buttonOnColourId, colours::accent);
    lowLatButton.setColour (juce::TextButton::textColourOffId, colours::dim);
    lowLatButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    lowLatButton.setTooltip ("Cuts plugin latency from ~15 ms to ~5 ms for live monitoring. "
                             "Slightly grainier on very low voices.");
    addAndMakeVisible (lowLatButton);
    lowLatAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "lowlat", lowLatButton);

    setSize (640, 400);
    startTimerHz (30);
}

ZavTuneEditor::~ZavTuneEditor() { setLookAndFeel (nullptr); }

void ZavTuneEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void ZavTuneEditor::timerCallback()
{
    shownHz = proc.detectedHz.load();
    shownNote = proc.targetNote.load();
    shownCorr += (proc.correction.load() - shownCorr) * 0.4f;
    bool low = proc.apvts.getRawParameterValue ("lowlat")->load() > 0.5f;
    if (low != shownLowLat) { shownLowLat = low; repaint(); }
    else repaint (displayArea);
}

void ZavTuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    // header
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("ZAV", 20, 12, 70, 34, juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.drawText ("TUNE", 78, 12, 100, 34, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("pitch correction", 20, 40, 200, 14, juce::Justification::centredLeft);
    {
        bool low = proc.apvts.getRawParameterValue ("lowlat")->load() > 0.5f;
        g.setColour (low ? colours::accent2 : colours::dim);
        g.drawText (low ? "latency ~5 ms" : "latency ~15 ms", getWidth() - 220, 18, 200, 20,
                    juce::Justification::centredRight);
    }

    // pitch display
    auto d = displayArea.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (d, 10.0f);

    bool voiced = shownHz > 0;
    g.setColour (voiced ? colours::text : colours::dim);
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
    g.drawText (voiced ? noteName (shownNote) : "--", d.removeFromLeft (120).toNearestInt(), juce::Justification::centred);

    auto info = d.removeFromLeft (110);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (colours::dim);
    g.drawText (voiced ? juce::String (shownHz, 1) + " Hz" : "no signal",
                info.removeFromTop (info.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft);
    g.drawText (voiced ? juce::String (juce::roundToInt (shownCorr * 100.0f)) + " cents" : "",
                info.toNearestInt(), juce::Justification::topLeft);

    // correction meter: -100..+100 cents
    auto meter = d.reduced (16, 0).withSizeKeepingCentre (d.getWidth() - 32, 10);
    g.setColour (colours::dim.withAlpha (0.3f));
    g.fillRoundedRectangle (meter, 5.0f);
    float cx = meter.getCentreX();
    g.setColour (colours::dim);
    g.fillRect (cx - 1.0f, meter.getY() - 6, 2.0f, meter.getHeight() + 12);
    if (voiced)
    {
        float v = juce::jlimit (-1.0f, 1.0f, shownCorr);
        float px = cx + v * meter.getWidth() * 0.5f;
        auto bar = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, px), meter.getY(), juce::jmax (cx, px), meter.getBottom());
        g.setGradientFill (juce::ColourGradient (colours::accent, meter.getX(), 0, colours::accent2, meter.getRight(), 0, false));
        g.fillRoundedRectangle (bar, 5.0f);
    }
}

void ZavTuneEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (48);

    auto top = area.removeFromTop (40);
    auto keyCol = top.removeFromLeft (110);
    keyLabel.setBounds (keyCol.removeFromLeft (40));
    keyBox.setBounds (keyCol.reduced (0, 6));
    top.removeFromLeft (16);
    auto scaleCol = top.removeFromLeft (230);
    scaleLabel.setBounds (scaleCol.removeFromLeft (50));
    scaleBox.setBounds (scaleCol.reduced (0, 6));
    lowLatButton.setBounds (top.removeFromRight (130).reduced (0, 6));

    area.removeFromTop (10);
    displayArea = area.removeFromTop (80);
    area.removeFromTop (16);

    Knob* knobs[] = { &retune, &amount, &humanize, &transpose, &mix };
    int w = area.getWidth() / 5;
    for (auto* k : knobs)
    {
        auto col = area.removeFromLeft (w);
        k->label.setBounds (col.removeFromTop (18));
        k->slider.setBounds (col);
    }
}

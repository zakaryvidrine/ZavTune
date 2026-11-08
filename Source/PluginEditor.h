#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ZavLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ZavLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class ZavTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ZavTuneEditor (ZavTuneProcessor&);
    ~ZavTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void setupKnob (Knob&, const juce::String& id, const juce::String& name);

    ZavTuneProcessor& proc;
    ZavLookAndFeel lnf;

    Knob retune, amount, humanize, transpose, mix;
    juce::ComboBox keyBox, scaleBox;
    juce::Label keyLabel, scaleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    juce::TextButton lowLatButton { "LOW LATENCY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lowLatAtt;

    float shownHz = 0, shownCorr = 0;
    int shownNote = -1;
    bool shownLowLat = false;
    juce::Rectangle<int> displayArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZavTuneEditor)
};

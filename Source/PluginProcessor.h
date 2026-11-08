#pragma once
#include <JuceHeader.h>
#include "ZavTuneDSP.h"

class ZavTuneProcessor : public juce::AudioProcessor
{
public:
    ZavTuneProcessor();
    ~ZavTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ZavTune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // read by the editor for the pitch display
    std::atomic<float> detectedHz { 0.0f };
    std::atomic<int>   targetNote { -1 };
    std::atomic<float> correction { 0.0f };

private:
    zav::AutoTuneEngine engine;
    double currentSampleRate = 44100.0;
    bool reportedLowLatency = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZavTuneProcessor)
};

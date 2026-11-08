#include "PluginProcessor.h"
#include "PluginEditor.h"

ZavTuneProcessor::ZavTuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ZavTuneProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    StringArray scales;
    for (int i = 0; i < (int) zav::Scale::NumScales; ++i) scales.add (zav::scaleName (i));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scales, 0));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "retune", 1 }, "Retune Speed",
        NormalisableRange<float> (0.0f, 400.0f, 0.1f, 0.4f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "transpose", 1 }, "Transpose",
        NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "lowlat", 1 }, "Low Latency", false));

    return { p.begin(), p.end() };
}

bool ZavTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void ZavTuneProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    bool low = apvts.getRawParameterValue ("lowlat")->load() > 0.5f;
    zav::Params p; p.lowLatency = low;
    engine.setParams (p);
    engine.prepare (sampleRate, getTotalNumOutputChannels());
    reportedLowLatency = low;
    setLatencySamples (zav::latencyFor (low, sampleRate));   // FL compensates via PDC
}

void ZavTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    zav::Params p;
    p.key       = (int) apvts.getRawParameterValue ("key")->load();
    p.scale     = (zav::Scale) (int) apvts.getRawParameterValue ("scale")->load();
    p.retuneMs  = apvts.getRawParameterValue ("retune")->load();
    p.amount    = apvts.getRawParameterValue ("amount")->load() * 0.01f;
    p.humanize  = apvts.getRawParameterValue ("humanize")->load() * 0.01f;
    p.transpose = apvts.getRawParameterValue ("transpose")->load();
    p.mix       = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    p.lowLatency = apvts.getRawParameterValue ("lowlat")->load() > 0.5f;
    engine.setParams (p);

    if (p.lowLatency != reportedLowLatency)
    {
        reportedLowLatency = p.lowLatency;
        setLatencySamples (zav::latencyFor (p.lowLatency, currentSampleRate));
    }

    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    detectedHz.store (engine.getDetectedHz());
    targetNote.store (engine.getTargetNote());
    correction.store (engine.getCurrentShift());
}

void ZavTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void ZavTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ZavTuneProcessor::createEditor() { return new ZavTuneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZavTuneProcessor(); }

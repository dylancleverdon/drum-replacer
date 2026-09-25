#include "PluginProcessor.h"
#include "PluginEditor.h"

KillroomAudioProcessor::KillroomAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "KillroomState", params::createLayout()),
      values (state)
{
}

bool KillroomAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void KillroomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    dsp.setSettings (values.read());
    setLatencySamples (dsp.getLatencySamples());
}

void KillroomAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    dsp.setSettings (values.read());

    if (dsp.getLatencySamples() != getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());

    dsp.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* KillroomAudioProcessor::createEditor()
{
    return new KillroomAudioProcessorEditor (*this);
}

void KillroomAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty ("savedByVersion", KILLROOM_VERSION_LABEL, nullptr);

    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void KillroomAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Parameters missing from older saves keep their defaults, so sets saved with an
    // earlier version load fine after an update.
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KillroomAudioProcessor();
}

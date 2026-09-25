#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "UpdateService.h"
#include "dsp/Killroom.h"

class KillroomAudioProcessor : public juce::AudioProcessor
{
public:
    KillroomAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
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

    juce::AudioProcessorValueTreeState state;
    killroom::MeterFifo& getMeterFifo() noexcept { return dsp.getMeterFifo(); }

    // Shared by every Killroom instance in the host, and kept alive by the processor
    // (not the editor) so closing the window doesn't interrupt an update.
    juce::SharedResourcePointer<UpdateService> updates;

private:
    params::Values values;
    killroom::Processor dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KillroomAudioProcessor)
};

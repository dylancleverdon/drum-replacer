#pragma once

#include "PluginProcessor.h"
#include "Ui.h"

class KillroomAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer,
                                     private juce::ChangeListener
{
public:
    explicit KillroomAudioProcessorEditor (KillroomAudioProcessor&);
    ~KillroomAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Colour colour;
        Control* left;
        Control* right;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void setupKnob (Control&, const char* parameterID, const juce::String& name, juce::Colour accent, bool bipolar);
    void setupBar (Control&, const char* parameterID, const juce::String& name, juce::Colour accent);
    void attach (Control&, const char* parameterID);
    void refreshUpdateStatus();
    void updateButtonClicked();

    KillroomAudioProcessor& processor;
    ui::LookAndFeel lookAndFeel;

    ui::EnvelopeDisplay display;
    Control attack, attackTime, release, releaseTime, roomKill, roomHold, output, mix, threshold, lookahead;
    std::array<Section, 4> sections;
    juce::Rectangle<int> footer;

    juce::TextButton versionButton, updateButton;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KillroomAudioProcessorEditor)
};

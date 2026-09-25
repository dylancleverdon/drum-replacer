#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 780, editorHeight = 480, margin = 16;
}

KillroomAudioProcessorEditor::KillroomAudioProcessorEditor (KillroomAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (display);

    setupKnob (attack,      params::attack,      "Gain",  ui::colours::shaper, true);
    setupKnob (attackTime,  params::attackTime,  "Time",  ui::colours::shaper, false);
    setupKnob (release,     params::release,     "Gain",  ui::colours::shaper, true);
    setupKnob (releaseTime, params::releaseTime, "Time",  ui::colours::shaper, false);
    setupKnob (roomKill,    params::roomKill,    "Kill",  ui::colours::kill,   false);
    setupKnob (roomHold,    params::roomHold,    "Hold",  ui::colours::kill,   false);
    setupKnob (output,      params::output,      "Level", ui::colours::output, true);
    setupKnob (mix,         params::mix,         "Mix",   ui::colours::output, false);
    setupBar (threshold,    params::threshold,   "THRESHOLD", ui::colours::text.withAlpha (0.55f));
    setupBar (lookahead,    params::lookahead,   "LOOKAHEAD", ui::colours::text.withAlpha (0.55f));

    sections = { Section { "ATTACK",      ui::colours::shaper, &attack,   &attackTime,  {} },
                 Section { "RELEASE",     ui::colours::shaper, &release,  &releaseTime, {} },
                 Section { "ROOM KILLER", ui::colours::kill,   &roomKill, &roomHold,    {} },
                 Section { "OUTPUT",      ui::colours::output, &output,   &mix,         {} } };

    versionButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    versionButton.setColour (juce::TextButton::textColourOffId, ui::colours::dimText);
    versionButton.getProperties().set ("rightAligned", true);
    versionButton.onClick = [this] { processor.updates->checkForUpdates (true); };
    addAndMakeVisible (versionButton);

    updateButton.onClick = [this] { updateButtonClicked(); };
    addChildComponent (updateButton);

    processor.updates->addChangeListener (this);
    processor.updates->checkForUpdates (false);
    refreshUpdateStatus();

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

KillroomAudioProcessorEditor::~KillroomAudioProcessorEditor()
{
    processor.updates->removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void KillroomAudioProcessorEditor::attach (Control& c, const char* parameterID)
{
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, parameterID, c.slider);

    if (auto* parameter = processor.state.getParameter (parameterID))
        c.slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
}

void KillroomAudioProcessorEditor::setupKnob (Control& c, const char* parameterID, const juce::String& name,
                                              juce::Colour accent, bool bipolar)
{
    c.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    c.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    ui::LookAndFeel::setAccent (c.slider, accent);
    ui::LookAndFeel::setBipolar (c.slider, bipolar);
    attach (c, parameterID);
    addAndMakeVisible (c.slider);

    c.label.setText (name, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centred);
    c.label.setFont (juce::FontOptions (12.0f));
    c.label.setColour (juce::Label::textColourId, ui::colours::dimText);
    addAndMakeVisible (c.label);
}

void KillroomAudioProcessorEditor::setupBar (Control& c, const char* parameterID, const juce::String& name, juce::Colour accent)
{
    c.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    c.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20);
    ui::LookAndFeel::setAccent (c.slider, accent);
    attach (c, parameterID);
    addAndMakeVisible (c.slider);

    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    c.label.setColour (juce::Label::textColourId, ui::colours::dimText);
    addAndMakeVisible (c.label);
}

//==============================================================================
void KillroomAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);

    g.setColour (ui::colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("KILLROOM", margin, 12, 160, 30, juce::Justification::centredLeft);
    g.setColour (ui::colours::dimText);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText ("transient shaper + room killer", margin + 132, 12, 220, 30, juce::Justification::centredLeft);

    auto drawPanel = [&g] (juce::Rectangle<int> r)
    {
        g.setColour (ui::colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (ui::colours::panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
    };

    for (auto& s : sections)
    {
        drawPanel (s.bounds);
        g.setColour (s.colour);
        g.fillRect (s.bounds.getX() + 12, s.bounds.getY() + 15, 3, 11);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.getX() + 21, s.bounds.getY() + 10, s.bounds.getWidth() - 30, 20, juce::Justification::centredLeft);
    }

    drawPanel (footer);
}

void KillroomAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (margin, 0);

    auto header = area.removeFromTop (54).withTrimmedTop (12).withTrimmedBottom (12);

    if (updateButton.isVisible())
    {
        updateButton.setBounds (header.removeFromRight (150));
        header.removeFromRight (8);
    }

    header.removeFromLeft (300); // title and subtitle
    versionButton.setBounds (header);

    display.setBounds (area.removeFromTop (166));
    area.removeFromTop (10);

    auto row = area.removeFromTop (178);
    const int gap = 10;
    const int sectionWidth = (row.getWidth() - gap * 3) / 4;

    for (auto& s : sections)
    {
        s.bounds = row.removeFromLeft (sectionWidth);
        row.removeFromLeft (gap);

        auto inner = s.bounds.reduced (8).withTrimmedTop (30);
        const int half = inner.getWidth() / 2;

        for (auto* c : { s.left, s.right })
        {
            auto column = inner.removeFromLeft (half);
            c->label.setBounds (column.removeFromBottom (18));
            c->slider.setBounds (column);
        }
    }

    area.removeFromTop (10);
    footer = area.removeFromTop (56);

    auto bars = footer.reduced (16, 0);
    const int barWidth = (bars.getWidth() - 32) / 2;

    for (auto* c : { &threshold, &lookahead })
    {
        auto bar = bars.removeFromLeft (barWidth);
        bars.removeFromLeft (32);
        c->label.setBounds (bar.removeFromLeft (84));
        c->slider.setBounds (bar.withSizeKeepingCentre (bar.getWidth(), 28));
    }
}

//==============================================================================
void KillroomAudioProcessorEditor::timerCallback()
{
    killroom::MeterFrame frame;

    while (processor.getMeterFifo().pop (frame))
        display.push (frame);

    display.setThreshold ((float) threshold.slider.getValue());
    display.repaint();
}

void KillroomAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshUpdateStatus();
}

void KillroomAudioProcessorEditor::updateButtonClicked()
{
    const auto status = processor.updates->getStatus();

    if (status.state == UpdateService::State::available
        || (status.state == UpdateService::State::failed && status.latestVersion.isNotEmpty()))
        processor.updates->installUpdate();
    else
        processor.updates->checkForUpdates (true);
}

void KillroomAudioProcessorEditor::refreshUpdateStatus()
{
    using State = UpdateService::State;

    const auto status = processor.updates->getStatus();
    const auto current = "v" + UpdateService::currentVersion();
    const auto latest = "v" + status.latestVersion;
    const bool canInstall = UpdateService::canInstall();

    juce::String text;
    juce::String button;

    switch (status.state)
    {
        case State::idle:
        case State::checking:   text = current + "  |  checking for updates..."; break;
        case State::upToDate:   text = current + "  |  up to date"; break;
        case State::noReleases: text = current + "  |  no releases published yet"; break;
        case State::installing: text = "Updating to " + latest + ": " + status.message; break;
        case State::installed:  text = latest + " installed  |  restart Live to use it"; break;
        case State::launched:   text = "Finish in the installer window, then restart Live"; break;

        case State::available:
            text = current + "  |  " + latest + " available";
            if (canInstall) button = "Update to " + latest;
            break;

        case State::failed:
            text = current + "  |  " + (status.latestVersion.isEmpty() ? "update check failed" : "update failed")
                 + (status.message.isNotEmpty() ? ": " + status.message : juce::String());
            button = "Retry";
            break;
    }

    versionButton.setButtonText (text);
    versionButton.setTooltip (status.message.isNotEmpty() ? status.message : "Click to check for updates");
    updateButton.setButtonText (button);

    if (updateButton.isVisible() != button.isNotEmpty())
    {
        updateButton.setVisible (button.isNotEmpty());
        resized();
    }
}

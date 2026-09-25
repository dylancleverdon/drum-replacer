#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/Killroom.h"

namespace ui
{
namespace colours
{
    inline const juce::Colour background { 0xff121418 };
    inline const juce::Colour panel      { 0xff1b1e24 };
    inline const juce::Colour panelEdge  { 0xff2a2f37 };
    inline const juce::Colour track      { 0xff30353e };
    inline const juce::Colour text       { 0xffdadee4 };
    inline const juce::Colour dimText    { 0xff8a919c };
    inline const juce::Colour shaper     { 0xffff9a3c };
    inline const juce::Colour kill       { 0xffff4f6a };
    inline const juce::Colour output     { 0xff54c6f0 };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    static void setAccent (juce::Slider&, juce::Colour);
    static void setBipolar (juce::Slider&, bool);
};

// Scrolling history of input level, output level and the gain being applied.
class EnvelopeDisplay : public juce::Component
{
public:
    void push (const killroom::MeterFrame& frame);
    void setThreshold (float db);
    void paint (juce::Graphics&) override;

private:
    static constexpr int historySize = 640; // 3.2 s of 5 ms frames
    std::array<killroom::MeterFrame, historySize> history {};
    int head = 0;
    float thresholdDb = -60.0f;
};
} // namespace ui

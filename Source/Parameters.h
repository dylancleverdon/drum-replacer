#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Killroom.h"

namespace params
{
// These IDs are stored in Live sets, presets and automation. Never rename or remove one,
// or saved sets will lose their settings after an update. New parameters get a new ID
// and a higher version hint (see createLayout).
inline constexpr auto attack      = "attack";
inline constexpr auto attackTime  = "attackTime";
inline constexpr auto release     = "release";
inline constexpr auto releaseTime = "releaseTime";
inline constexpr auto threshold   = "threshold";
inline constexpr auto roomKill    = "roomKill";
inline constexpr auto roomHold    = "roomHold";
inline constexpr auto lookahead   = "lookahead";
inline constexpr auto output      = "output";
inline constexpr auto mix         = "mix";

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Cached raw parameter pointers so the audio thread doesn't look anything up by name.
struct Values
{
    explicit Values (juce::AudioProcessorValueTreeState& state);
    killroom::Settings read() const noexcept;

    std::atomic<float>* attack;
    std::atomic<float>* attackTime;
    std::atomic<float>* release;
    std::atomic<float>* releaseTime;
    std::atomic<float>* threshold;
    std::atomic<float>* roomKill;
    std::atomic<float>* roomHold;
    std::atomic<float>* lookahead;
    std::atomic<float>* output;
    std::atomic<float>* mix;
};
} // namespace params

#include "Parameters.h"

namespace params
{
namespace
{
    using Attributes = juce::AudioParameterFloatAttributes;

    juce::String percentText (float v, bool bipolar)
    {
        const auto rounded = juce::roundToInt (v);
        return (bipolar && rounded > 0 ? "+" : "") + juce::String (rounded) + " %";
    }

    juce::String msText (float v)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
        if (v >= 10.0f)   return juce::String (juce::roundToInt (v)) + " ms";
        return juce::String (v, 1) + " ms";
    }

    juce::String dbText (float v)
    {
        return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    }

    float parseMs (const juce::String& text)
    {
        const auto value = text.retainCharacters ("0123456789.-").getFloatValue();
        return text.trim().endsWithIgnoreCase ("s") && ! text.trim().endsWithIgnoreCase ("ms") ? value * 1000.0f : value;
    }

    float parseNumber (const juce::String& text)
    {
        return text.retainCharacters ("0123456789.-").getFloatValue();
    }

    juce::NormalisableRange<float> timeRange (float min, float max, float centre)
    {
        juce::NormalisableRange<float> range (min, max, 0.01f);
        range.setSkewForCentre (centre);
        return range;
    }

    Attributes percent (bool bipolar)
    {
        return Attributes().withStringFromValueFunction ([bipolar] (float v, int) { return percentText (v, bipolar); })
                           .withValueFromStringFunction (parseNumber)
                           .withLabel ("%");
    }

    Attributes milliseconds()
    {
        return Attributes().withStringFromValueFunction ([] (float v, int) { return msText (v); })
                           .withValueFromStringFunction (parseMs)
                           .withLabel ("ms");
    }

    Attributes decibels()
    {
        return Attributes().withStringFromValueFunction ([] (float v, int) { return dbText (v); })
                           .withValueFromStringFunction (parseNumber)
                           .withLabel ("dB");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat;
    constexpr int v1 = 1; // version hint for the parameters that shipped in 1.0

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P> (juce::ParameterID { attack, v1 }, "Attack",
                                     juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, percent (true)));
    layout.add (std::make_unique<P> (juce::ParameterID { attackTime, v1 }, "Attack Time",
                                     timeRange (1.0f, 200.0f, 20.0f), 20.0f, milliseconds()));
    layout.add (std::make_unique<P> (juce::ParameterID { release, v1 }, "Release",
                                     juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, percent (true)));
    layout.add (std::make_unique<P> (juce::ParameterID { releaseTime, v1 }, "Release Time",
                                     timeRange (10.0f, 2000.0f, 250.0f), 250.0f, milliseconds()));
    layout.add (std::make_unique<P> (juce::ParameterID { threshold, v1 }, "Threshold",
                                     juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -60.0f, decibels()));
    layout.add (std::make_unique<P> (juce::ParameterID { roomKill, v1 }, "Room Kill",
                                     juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, percent (false)));
    layout.add (std::make_unique<P> (juce::ParameterID { roomHold, v1 }, "Room Hold",
                                     timeRange (5.0f, 500.0f, 60.0f), 60.0f, milliseconds()));
    // Changing the lookahead changes the plugin's latency, so it isn't automatable.
    layout.add (std::make_unique<P> (juce::ParameterID { lookahead, v1 }, "Lookahead",
                                     juce::NormalisableRange<float> (0.0f, killroom::Processor::maxLookaheadMs, 0.1f), 2.0f,
                                     milliseconds().withAutomatable (false)));
    layout.add (std::make_unique<P> (juce::ParameterID { output, v1 }, "Output",
                                     juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, decibels()));
    layout.add (std::make_unique<P> (juce::ParameterID { mix, v1 }, "Mix",
                                     juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, percent (false)));

    return layout;
}

Values::Values (juce::AudioProcessorValueTreeState& state)
    : attack      (state.getRawParameterValue (params::attack)),
      attackTime  (state.getRawParameterValue (params::attackTime)),
      release     (state.getRawParameterValue (params::release)),
      releaseTime (state.getRawParameterValue (params::releaseTime)),
      threshold   (state.getRawParameterValue (params::threshold)),
      roomKill    (state.getRawParameterValue (params::roomKill)),
      roomHold    (state.getRawParameterValue (params::roomHold)),
      lookahead   (state.getRawParameterValue (params::lookahead)),
      output      (state.getRawParameterValue (params::output)),
      mix         (state.getRawParameterValue (params::mix))
{
}

killroom::Settings Values::read() const noexcept
{
    killroom::Settings s;
    s.attackPercent  = attack->load();
    s.attackTimeMs   = attackTime->load();
    s.releasePercent = release->load();
    s.releaseTimeMs  = releaseTime->load();
    s.thresholdDb    = threshold->load();
    s.killPercent    = roomKill->load();
    s.holdMs         = roomHold->load();
    s.lookaheadMs    = lookahead->load();
    s.outputDb       = output->load();
    s.mixPercent     = mix->load();
    return s;
}
} // namespace params

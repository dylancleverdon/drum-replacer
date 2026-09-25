#pragma once

#include <array>
#include <atomic>
#include <vector>

// The Killroom DSP core. Deliberately free of JUCE so it can be unit tested
// (tests/DspTests.cpp) and reasoned about on its own.
//
// Signal flow per sample:
//   detector  = max |x| across channels (stereo linked, undelayed)
//   level     = peak-hold envelope of the detector (instant attack, ripple free)
//   attack    = how far `level` is above a copy of itself that rises with the
//               Attack Time        -> boosts/cuts the start of each hit
//   release   = how far `level` has fallen below a copy of itself that decays
//               with the Release Time -> boosts/cuts the body/tail of each hit
//   room kill = after each detected hit plus the Hold time, the tail's decay is
//               multiplied (level-independent), and anything below Threshold is
//               pushed down by the full kill depth
//   audio     = input delayed by the lookahead, multiplied by the combined gain
namespace killroom
{

struct Settings
{
    float attackPercent  = 0.0f;   // -100 .. 100
    float attackTimeMs   = 20.0f;  // how long the attack phase lasts
    float releasePercent = 0.0f;   // -100 .. 100
    float releaseTimeMs  = 250.0f; // decay rate the release section compares against
    float thresholdDb    = -60.0f; // hits below this are left alone by the shaper and treated as room by the killer
    float killPercent    = 0.0f;   // 0 .. 100
    float holdMs         = 60.0f;  // how long each hit rings before the room killer acts
    float lookaheadMs    = 2.0f;   // 0 .. maxLookaheadMs, adds latency
    float outputDb       = 0.0f;
    float mixPercent     = 100.0f;
};

// One display frame (~5 ms of audio). Written on the audio thread, read by the UI.
struct MeterFrame
{
    float inputDb  = -120.0f; // peak of the (delayed) input
    float outputDb = -120.0f; // peak of the output
    float gainDb   = 0.0f;    // most extreme combined gain in the frame
    float killDb   = 0.0f;    // deepest room-killer reduction in the frame
};

// Single producer / single consumer queue for meter frames.
class MeterFifo
{
public:
    void push (const MeterFrame& frame) noexcept;
    bool pop (MeterFrame& frame) noexcept;

private:
    static constexpr int capacity = 1024;
    std::array<MeterFrame, capacity> frames {};
    std::atomic<int> writeIndex { 0 }, readIndex { 0 };
};

class Processor
{
public:
    static constexpr float maxLookaheadMs = 10.0f;
    static constexpr float maxShapeDb     = 24.0f; // per-section gain limit
    static constexpr float maxKillDepthDb = 90.0f; // room killer depth at 100 %
    static constexpr float maxKillSlope   = 8.0f;  // tail decays (1 + slope) times faster at 100 %
    static constexpr float onsetRiseDb    = 5.0f;  // rise that counts as a new hit
    static constexpr float onsetRearmDb   = 1.5f;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // Call before process() whenever parameters may have changed (once per block is fine).
    void setSettings (const Settings& newSettings);

    // Latency in samples introduced by the current lookahead.
    int getLatencySamples() const noexcept { return latencySamples; }

    // Processes numChannels channels of numSamples samples in place.
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    MeterFifo& getMeterFifo() noexcept { return meterFifo; }

private:
    struct Smoothed
    {
        float current = 0.0f, target = 0.0f, step = 0.0f;
        int remaining = 0;

        void setImmediate (float v) noexcept { current = target = v; remaining = 0; step = 0.0f; }
        void setTarget (float v, int rampSamples) noexcept;
        float next() noexcept;
    };

    void updateCoefficients() noexcept;
    float coefficientFor (float timeMs) const noexcept;
    void pushMeter (float inPeak, float outPeak, float gainDb, float killDb) noexcept;

    double sampleRate = 44100.0;
    Settings settings;
    bool snapSmoothers = true;

    // Delay line (lookahead)
    std::vector<std::vector<float>> delay;
    int delaySize = 1, writePos = 0, latencySamples = 0;

    // Detector state
    float level = 0.0f;         // peak-hold envelope
    int peakHoldLeft = 0;
    float attackFollower = 0.0f;
    float onsetFollower = 0.0f;
    float releaseFollower = 0.0f;

    // Room killer state
    bool onsetArmed = true;
    int killHoldLeft = 0;
    float killReference = 0.0f;
    float killGainDb = 0.0f;
    float shaperGainDb = 0.0f;

    // Coefficients
    int peakHoldSamples = 1, killHoldSamples = 1;
    float levelRelease = 0.0f, attackCoeff = 0.0f, onsetCoeff = 0.0f, releaseCoeff = 0.0f;
    float killOpenFast = 0.0f, killOpenSlow = 0.0f, killClose = 0.0f, shaperSmooth = 0.0f;

    // Smoothed per-sample parameters
    Smoothed attackAmount, releaseAmount, killAmount, mixAmount, outputGain;
    int rampSamples = 1;

    // Metering
    MeterFifo meterFifo;
    int meterChunk = 256, meterCount = 0;
    float meterIn = 0.0f, meterOut = 0.0f, meterGain = 0.0f, meterKill = 0.0f;
};

} // namespace killroom

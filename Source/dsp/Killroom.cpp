#include "Killroom.h"

#include <algorithm>
#include <cmath>

namespace killroom
{

namespace
{
    constexpr float kneeDb          = 6.0f;
    constexpr float peakHoldMs      = 12.0f;  // longer than half a period of a 40 Hz kick, so the level doesn't ripple
    constexpr float levelReleaseMs  = 20.0f;
    constexpr float onsetTimeMs     = 8.0f;
    constexpr float killOpenFastMs  = 0.1f;   // opening for a new hit (happens inside the lookahead)
    constexpr float killOpenSlowMs  = 50.0f;  // letting go in the tail, slow so noisy tails don't chatter
    constexpr float killCloseMs     = 8.0f;
    constexpr float shaperSmoothMs  = 0.3f;
    constexpr float parameterRampMs = 20.0f;
    constexpr float meterFrameMs    = 5.0f;
    constexpr float silence         = 1.0e-9f;

    inline float toDb (float linear) noexcept
    {
        return 20.0f * std::log10 (std::max (linear, 1.0e-6f)); // floor at -120 dB
    }

    inline float dbToGain (float db) noexcept
    {
        return std::exp (db * 0.11512925464970229f); // ln(10) / 20
    }

    inline float clamp01 (float x) noexcept { return std::min (1.0f, std::max (0.0f, x)); }

    inline float clampShape (float db) noexcept
    {
        return std::min (Processor::maxShapeDb, std::max (-Processor::maxShapeDb, db));
    }
}

//==============================================================================
void MeterFifo::push (const MeterFrame& frame) noexcept
{
    const int write = writeIndex.load (std::memory_order_relaxed);
    const int next = (write + 1) % capacity;

    if (next == readIndex.load (std::memory_order_acquire))
        return; // full: the UI isn't reading (editor closed), drop the frame

    frames[(size_t) write] = frame;
    writeIndex.store (next, std::memory_order_release);
}

bool MeterFifo::pop (MeterFrame& frame) noexcept
{
    const int read = readIndex.load (std::memory_order_relaxed);

    if (read == writeIndex.load (std::memory_order_acquire))
        return false;

    frame = frames[(size_t) read];
    readIndex.store ((read + 1) % capacity, std::memory_order_release);
    return true;
}

//==============================================================================
void Processor::Smoothed::setTarget (float v, int rampSamples) noexcept
{
    if (std::abs (v - target) < 1.0e-9f)
        return;

    target = v;
    remaining = std::max (1, rampSamples);
    step = (target - current) / (float) remaining;
}

float Processor::Smoothed::next() noexcept
{
    if (remaining > 0)
    {
        if (--remaining == 0)
            current = target;
        else
            current += step;
    }

    return current;
}

//==============================================================================
void Processor::prepare (double newSampleRate, int /*maxBlockSize*/, int numChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    delaySize = (int) std::ceil (maxLookaheadMs * 0.001 * sampleRate) + 1;
    delay.assign ((size_t) std::max (1, numChannels), std::vector<float> ((size_t) delaySize, 0.0f));

    rampSamples = std::max (1, (int) std::lround (parameterRampMs * 0.001 * sampleRate));
    meterChunk  = std::max (1, (int) std::lround (meterFrameMs * 0.001 * sampleRate));

    setSettings (settings);  // recompute coefficients and latency for the new rate
    snapSmoothers = true;    // the host's first real settings apply immediately, not ramped
    reset();
}

void Processor::reset()
{
    for (auto& channel : delay)
        std::fill (channel.begin(), channel.end(), 0.0f);

    writePos = 0;
    level = attackFollower = onsetFollower = releaseFollower = 0.0f;
    peakHoldLeft = 0;
    onsetArmed = true;
    killHoldLeft = 0;
    killReference = 0.0f;
    killGainDb = shaperGainDb = 0.0f;

    for (auto* s : { &attackAmount, &releaseAmount, &killAmount, &mixAmount, &outputGain })
        s->setImmediate (s->target);

    meterCount = 0;
    meterIn = meterOut = meterGain = meterKill = 0.0f;
}

float Processor::coefficientFor (float timeMs) const noexcept
{
    if (timeMs <= 0.0f)
        return 0.0f;

    return (float) std::exp (-1.0 / (timeMs * 0.001 * sampleRate));
}

void Processor::updateCoefficients() noexcept
{
    peakHoldSamples = std::max (1, (int) std::lround (peakHoldMs * 0.001 * sampleRate));
    levelRelease    = coefficientFor (levelReleaseMs);
    onsetCoeff      = coefficientFor (onsetTimeMs);
    killOpenFast    = coefficientFor (killOpenFastMs);
    killOpenSlow    = coefficientFor (killOpenSlowMs);
    killClose       = coefficientFor (killCloseMs);
    shaperSmooth    = coefficientFor (shaperSmoothMs);

    attackCoeff     = coefficientFor (std::max (0.1f, settings.attackTimeMs));
    releaseCoeff    = coefficientFor (std::max (1.0f, settings.releaseTimeMs));
    killHoldSamples = std::max (1, (int) std::lround (settings.holdMs * 0.001 * sampleRate));
}

void Processor::setSettings (const Settings& newSettings)
{
    const bool snap = snapSmoothers;
    snapSmoothers = false;
    settings = newSettings;

    updateCoefficients();

    const int wantedLatency = (int) std::lround (std::max (0.0f, settings.lookaheadMs) * 0.001 * sampleRate);
    latencySamples = std::min (wantedLatency, delaySize - 1);

    auto set = [&] (Smoothed& s, float value)
    {
        if (snap)
            s.setImmediate (value);
        else
            s.setTarget (value, rampSamples);
    };

    set (attackAmount,  std::min (1.0f, std::max (-1.0f, settings.attackPercent * 0.01f)));
    set (releaseAmount, std::min (1.0f, std::max (-1.0f, settings.releasePercent * 0.01f)));
    set (killAmount,    clamp01 (settings.killPercent * 0.01f));
    set (mixAmount,     clamp01 (settings.mixPercent * 0.01f));
    set (outputGain,    dbToGain (settings.outputDb));
}

//==============================================================================
void Processor::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int chans = std::min (numChannels, (int) delay.size());
    const float threshold = settings.thresholdDb;

    for (int n = 0; n < numSamples; ++n)
    {
        // Detector (undelayed) + write into the lookahead delay line
        float detector = 0.0f;

        for (int c = 0; c < chans; ++c)
        {
            const float x = channels[c][n];
            detector = std::max (detector, std::abs (x));
            delay[(size_t) c][(size_t) writePos] = x;
        }

        // Level: instant attack, hold, then release. The hold keeps low notes from rippling.
        if (detector >= level)
        {
            level = detector;
            peakHoldLeft = peakHoldSamples;
        }
        else if (peakHoldLeft > 0)
        {
            --peakHoldLeft;
        }
        else
        {
            level = detector + levelRelease * (level - detector);
            if (level < silence) level = 0.0f;
        }

        const float levelDb = toDb (level);

        // Attack section: a follower that rises at the attack time lags behind the level at
        // the start of each hit. The gap (in dB) is the "attack" portion of the envelope.
        if (level > attackFollower)
            attackFollower = level + attackCoeff * (attackFollower - level);
        else
            attackFollower = level;

        const float attackDeltaDb = levelDb - toDb (attackFollower);

        // Onset detection for the room killer, with its own fixed timing.
        if (level > onsetFollower)
            onsetFollower = level + onsetCoeff * (onsetFollower - level);
        else
            onsetFollower = level;

        const float riseDb = levelDb - toDb (onsetFollower);

        // Release section: a follower that decays at the release time. When the hit dies away
        // faster than that, the gap (in dB) is the "release" portion of the envelope.
        if (level >= releaseFollower)
        {
            releaseFollower = level;
        }
        else
        {
            releaseFollower *= releaseCoeff;
            if (releaseFollower < silence) releaseFollower = 0.0f;
        }

        const float releaseFollowerDb = toDb (releaseFollower);
        const float sustainDeltaDb = std::max (0.0f, releaseFollowerDb - levelDb);

        // Shaper gain, faded out below the threshold. The release follower remembers the hit,
        // so the tail of a hit that crossed the threshold is still shaped.
        const float weight = clamp01 ((releaseFollowerDb - threshold) / kneeDb + 0.5f);
        const float attackAmt = attackAmount.next();
        const float releaseAmt = releaseAmount.next();
        const float shaperTarget = weight * (clampShape (attackAmt * attackDeltaDb)
                                             + clampShape (releaseAmt * sustainDeltaDb));
        shaperGainDb = shaperTarget + shaperSmooth * (shaperGainDb - shaperTarget);

        // Room killer
        const float kill = killAmount.next();

        if (onsetArmed && riseDb > onsetRiseDb && levelDb > threshold)
        {
            onsetArmed = false;
            killHoldLeft = killHoldSamples;
            killReference = level;
        }
        else if (! onsetArmed && riseDb < onsetRearmDb)
        {
            onsetArmed = true;
        }

        float killTargetDb = 0.0f;
        bool holding = false;

        if (killHoldLeft > 0)
        {
            holding = true;

            // Measure the tail from where the level is when the hold ends, so the
            // reduction starts at 0 dB and grows smoothly as the room decays.
            if (--killHoldLeft == 0)
                killReference = level;
        }
        else
        {
            killReference = std::max (killReference, level);

            const float depth = kill * maxKillDepthDb;
            const float dropDb = toDb (killReference) - levelDb;
            const float belowThreshold = clamp01 ((threshold - levelDb) / kneeDb + 0.5f);
            killTargetDb = -std::min (depth, std::max (kill * maxKillSlope * dropDb, belowThreshold * depth));
        }

        const float killCoeff = killTargetDb > killGainDb ? (holding ? killOpenFast : killOpenSlow)
                                                          : killClose;
        killGainDb = killTargetDb + killCoeff * (killGainDb - killTargetDb);

        // Apply to the delayed audio
        const float totalDb = shaperGainDb + killGainDb;
        const float mix = mixAmount.next();
        const float finalGain = outputGain.next() * (1.0f - mix + mix * dbToGain (totalDb));

        int readPos = writePos - latencySamples;
        if (readPos < 0)
            readPos += delaySize;

        float inPeak = 0.0f, outPeak = 0.0f;

        for (int c = 0; c < chans; ++c)
        {
            const float x = delay[(size_t) c][(size_t) readPos];
            const float y = x * finalGain;
            channels[c][n] = y;
            inPeak = std::max (inPeak, std::abs (x));
            outPeak = std::max (outPeak, std::abs (y));
        }

        if (++writePos == delaySize)
            writePos = 0;

        pushMeter (inPeak, outPeak, totalDb, killGainDb);
    }
}

void Processor::pushMeter (float inPeak, float outPeak, float gainDb, float killDb) noexcept
{
    meterIn = std::max (meterIn, inPeak);
    meterOut = std::max (meterOut, outPeak);
    if (std::abs (gainDb) > std::abs (meterGain)) meterGain = gainDb;
    meterKill = std::min (meterKill, killDb);

    if (++meterCount < meterChunk)
        return;

    meterFifo.push ({ toDb (meterIn), toDb (meterOut), meterGain, meterKill });
    meterCount = 0;
    meterIn = meterOut = meterGain = meterKill = 0.0f;
}

} // namespace killroom

// Behavioural tests for the Killroom DSP core. Built and run by CTest (locally and in CI).
// Each test renders a synthetic drum track (hits with a body plus a decaying "room" tail)
// through the processor and checks the effect on specific time windows.

#include "../Source/dsp/Killroom.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using killroom::Processor;
using killroom::Settings;

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::printf ("  [%s] %s\n", condition ? "ok" : "FAIL", what.c_str());
    if (! condition)
        ++failures;
}

std::string fmt (const char* format, double a, double b = 0.0)
{
    char buffer[256];
    std::snprintf (buffer, sizeof (buffer), format, a, b);
    return buffer;
}

struct Hit
{
    double timeSec;
    float gainDb;
};

// A snare-like hit: tone + noise with a 40 ms body, then a room tail 18 dB down
// with a 0.9 s RT60 that starts a few ms after the hit.
std::vector<float> makeDrums (double sr, double lengthSec, const std::vector<Hit>& hits,
                              float roomDb = -18.0f, unsigned seed = 1)
{
    std::vector<float> out ((size_t) (lengthSec * sr), 0.0f);
    std::mt19937 rng (seed);
    std::normal_distribution<float> noise (0.0f, 1.0f);
    const double pi = 3.14159265358979323846;
    const double roomTau = 0.9 / 6.91; // RT60 -> time constant

    for (const auto& hit : hits)
    {
        const auto start = (size_t) (hit.timeSec * sr);
        const float gain = std::pow (10.0f, hit.gainDb / 20.0f);
        const float room = std::pow (10.0f, roomDb / 20.0f);
        float lp = 0.0f;

        for (size_t i = start; i < out.size(); ++i)
        {
            const double t = (double) (i - start) / sr;
            const double body = std::exp (-t / 0.040) * std::min (1.0, t / 0.0003);
            const float tone = (float) std::sin (2.0 * pi * 180.0 * t);
            const float n = noise (rng);
            lp += 0.3f * (n - lp); // darker noise for the room

            const double roomEnv = t < 0.004 ? 0.0 : std::exp (-(t - 0.004) / roomTau) * std::min (1.0, (t - 0.004) / 0.010);
            out[i] += gain * (float) (body * (0.6 * tone + 0.4 * n) + room * roomEnv * lp * 2.0);
        }
    }

    return out;
}

struct Render
{
    std::vector<float> input, output; // output already shifted back by the latency
    int latency = 0;
};

Render render (const std::vector<float>& mono, const Settings& settings, double sr, int blockSize = 256, int channels = 2)
{
    Processor proc;
    proc.prepare (sr, blockSize, channels);
    proc.setSettings (settings);

    const int latency = proc.getLatencySamples();
    const size_t total = mono.size() + (size_t) latency;
    std::vector<std::vector<float>> buffers ((size_t) channels, std::vector<float> (total, 0.0f));

    for (int c = 0; c < channels; ++c)
        std::copy (mono.begin(), mono.end(), buffers[(size_t) c].begin());

    for (size_t pos = 0; pos < total; pos += (size_t) blockSize)
    {
        const int n = (int) std::min ((size_t) blockSize, total - pos);
        std::vector<float*> ptrs;
        for (auto& b : buffers)
            ptrs.push_back (b.data() + pos);
        proc.process (ptrs.data(), channels, n);
    }

    Render r;
    r.input = mono;
    r.latency = latency;
    r.output.assign (buffers[0].begin() + latency, buffers[0].begin() + latency + (long) mono.size());
    return r;
}

double rmsDb (const std::vector<float>& x, double sr, double fromSec, double toSec)
{
    const auto a = (size_t) (fromSec * sr);
    const auto b = std::min (x.size(), (size_t) (toSec * sr));
    double sum = 0.0;
    for (size_t i = a; i < b; ++i)
        sum += (double) x[i] * x[i];
    return 10.0 * std::log10 (sum / (double) std::max<size_t> (1, b - a) + 1e-30);
}

// Output level minus input level over a window, in dB.
double changeDb (const Render& r, double sr, double fromSec, double toSec)
{
    return rmsDb (r.output, sr, fromSec, toSec) - rmsDb (r.input, sr, fromSec, toSec);
}

const double sr = 48000.0;
const std::vector<Hit> fourHits { { 0.1, 0.0f }, { 0.7, -3.0f }, { 1.3, -1.0f }, { 1.9, -6.0f } };

// Average change over the same window of every hit.
double hitWindowChange (const Render& r, double from, double to)
{
    double sum = 0.0;
    for (const auto& h : fourHits)
        sum += changeDb (r, sr, h.timeSec + from, h.timeSec + to);
    return sum / (double) fourHits.size();
}

//==============================================================================
void testTransparentAtDefaults()
{
    std::printf ("Defaults are transparent (just delayed)\n");
    const auto drums = makeDrums (sr, 2.5, fourHits);
    Settings s;
    const auto r = render (drums, s, sr);

    float maxErr = 0.0f;
    for (size_t i = 0; i < drums.size(); ++i)
        maxErr = std::max (maxErr, std::abs (r.output[i] - drums[i]));

    check (r.latency == (int) std::lround (s.lookaheadMs * 0.001 * sr), fmt ("latency matches lookahead (%.0f samples)", r.latency));
    check (maxErr < 1.0e-6f, fmt ("output == delayed input (max error %.2e)", maxErr));
}

void testLookaheadLatency()
{
    std::printf ("Lookahead sets latency and delays the audio exactly\n");
    std::vector<float> impulse (4800, 0.0f);
    impulse[100] = 1.0f;

    for (float ms : { 0.0f, 1.0f, 5.0f, 10.0f })
    {
        Settings s;
        s.lookaheadMs = ms;
        Processor proc;
        proc.prepare (sr, 512, 1);
        proc.setSettings (s);
        std::vector<float> buf = impulse;
        float* ptr = buf.data();
        proc.process (&ptr, 1, (int) buf.size());

        const auto expected = (int) std::lround (ms * 0.001 * sr);
        size_t peak = 0;
        for (size_t i = 0; i < buf.size(); ++i)
            if (std::abs (buf[i]) > std::abs (buf[peak]))
                peak = i;

        check (proc.getLatencySamples() == expected && (int) peak == 100 + expected,
               fmt ("%.0f ms lookahead -> impulse moved by %.0f samples", ms, (double) peak - 100));
    }
}

void testAttack()
{
    std::printf ("Attack gain shapes the start of each hit\n");
    const auto drums = makeDrums (sr, 2.5, fourHits);

    Settings up;
    up.attackPercent = 100.0f;
    const auto rUp = render (drums, up, sr);
    const double upEarly = hitWindowChange (rUp, 0.0, 0.005);
    const double upTail = hitWindowChange (rUp, 0.2, 0.5);
    check (upEarly > 4.0, fmt ("+100%%: first 5 ms louder (%+.1f dB)", upEarly));
    check (std::abs (upTail) < 1.0, fmt ("+100%%: tail untouched (%+.2f dB)", upTail));

    Settings down;
    down.attackPercent = -100.0f;
    const auto rDown = render (drums, down, sr);
    const double downEarly = hitWindowChange (rDown, 0.0, 0.005);
    check (downEarly < -4.0, fmt ("-100%%: first 5 ms quieter (%+.1f dB)", downEarly));

    Settings half;
    half.attackPercent = 50.0f;
    const double halfEarly = hitWindowChange (render (drums, half, sr), 0.0, 0.005);
    check (halfEarly > 1.0 && halfEarly < upEarly, fmt ("+50%% sits between 0 and +100%% (%+.1f dB)", halfEarly));

    Settings longer = up;
    longer.attackTimeMs = 80.0f;
    const double longBody = hitWindowChange (render (drums, longer, sr), 0.02, 0.05);
    const double shortBody = hitWindowChange (rUp, 0.02, 0.05);
    check (longBody > shortBody + 1.0, fmt ("longer attack time boosts more of the body (%+.1f vs %+.1f dB)", longBody, shortBody));
}

void testRelease()
{
    std::printf ("Release gain shapes the body and tail\n");
    const auto drums = makeDrums (sr, 2.5, fourHits);

    Settings cut;
    cut.releasePercent = -100.0f;
    const auto rCut = render (drums, cut, sr);
    const double cutTail = hitWindowChange (rCut, 0.15, 0.5);
    const double cutEarly = hitWindowChange (rCut, 0.0, 0.004);
    check (cutTail < -6.0, fmt ("-100%%: tail quieter (%+.1f dB)", cutTail));
    check (std::abs (cutEarly) < 1.5, fmt ("-100%%: attack kept (%+.2f dB)", cutEarly));

    Settings boost;
    boost.releasePercent = 100.0f;
    const double boostTail = hitWindowChange (render (drums, boost, sr), 0.15, 0.5);
    check (boostTail > 3.0, fmt ("+100%%: tail louder (%+.1f dB)", boostTail));
}

void testRoomKiller()
{
    std::printf ("Room killer removes the tail but keeps the hit\n");
    const auto drums = makeDrums (sr, 2.5, fourHits);

    Settings full;
    full.killPercent = 100.0f;
    full.holdMs = 60.0f;
    const auto rFull = render (drums, full, sr);
    const double body = hitWindowChange (rFull, 0.0, 0.05);
    const double tail = hitWindowChange (rFull, 0.2, 0.55);
    check (std::abs (body) < 1.0, fmt ("100%%: first 50 ms kept (%+.2f dB)", body));
    check (tail < -30.0, fmt ("100%%: room tail killed (%+.1f dB)", tail));

    Settings half = full;
    half.killPercent = 40.0f;
    const double halfTail = hitWindowChange (render (drums, half, sr), 0.2, 0.55);
    check (halfTail < -6.0 && halfTail > tail, fmt ("40%%: partial reduction (%+.1f dB)", halfTail));

    Settings shortHold = full;
    shortHold.holdMs = 10.0f;
    const double shortBody = hitWindowChange (render (drums, shortHold, sr), 0.03, 0.06);
    const double longBody = hitWindowChange (rFull, 0.03, 0.06);
    check (shortBody < longBody - 3.0, fmt ("shorter hold tightens the body (%+.1f vs %+.1f dB)", shortBody, longBody));
}

void testGhostNotesSurvive()
{
    std::printf ("Room killer keeps quiet hits that land in a loud hit's tail\n");
    const std::vector<Hit> hits { { 0.1, 0.0f }, { 0.35, -20.0f }, { 0.9, 0.0f } };
    const auto drums = makeDrums (sr, 1.5, hits);

    Settings s;
    s.killPercent = 100.0f;
    s.holdMs = 50.0f;
    const auto r = render (drums, s, sr);
    const double ghost = changeDb (r, sr, 0.35, 0.38);
    check (std::abs (ghost) < 1.5, fmt ("ghost note at -20 dB kept (%+.2f dB)", ghost));
}

void testThreshold()
{
    std::printf ("Threshold leaves quiet material alone\n");
    const std::vector<Hit> quiet { { 0.1, -40.0f }, { 0.7, -40.0f } };
    const auto drums = makeDrums (sr, 1.2, quiet);

    Settings s;
    s.attackPercent = 100.0f;
    s.releasePercent = -100.0f;
    s.thresholdDb = -20.0f;
    const auto r = render (drums, s, sr);
    const double change = changeDb (r, sr, 0.1, 0.4);
    check (std::abs (change) < 0.5, fmt ("hits 20 dB under the threshold unchanged (%+.2f dB)", change));

    s.thresholdDb = -70.0f;
    const double active = changeDb (render (drums, s, sr), sr, 0.1, 0.105);
    check (active > 3.0, fmt ("same hits shaped once above the threshold (%+.1f dB)", active));
}

void testMixAndOutput()
{
    std::printf ("Mix and output\n");
    const auto drums = makeDrums (sr, 1.0, { { 0.1, 0.0f } });

    Settings dry;
    dry.attackPercent = 100.0f;
    dry.killPercent = 100.0f;
    dry.mixPercent = 0.0f;
    const auto r = render (drums, dry, sr);
    float maxErr = 0.0f;
    for (size_t i = 0; i < drums.size(); ++i)
        maxErr = std::max (maxErr, std::abs (r.output[i] - drums[i]));
    check (maxErr < 1.0e-6f, fmt ("0%% mix = dry signal (max error %.2e)", maxErr));

    Settings loud;
    loud.outputDb = 6.0f;
    const double gain = changeDb (render (drums, loud, sr), sr, 0.0, 1.0);
    check (std::abs (gain - 6.0) < 0.01, fmt ("+6 dB output (%+.3f dB)", gain));
}

void testSampleRateIndependence()
{
    std::printf ("Behaviour is consistent across sample rates\n");
    Settings s;
    s.attackPercent = 80.0f;
    s.releasePercent = -60.0f;
    s.killPercent = 60.0f;

    std::vector<double> early, tail;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const auto drums = makeDrums (rate, 1.5, { { 0.1, 0.0f }, { 0.8, -4.0f } });
        const auto r = render (drums, s, rate);
        early.push_back (changeDb (r, rate, 0.1, 0.108));
        tail.push_back (changeDb (r, rate, 0.3, 0.6));
    }

    check (std::abs (early[0] - early[2]) < 1.5 && std::abs (early[1] - early[2]) < 1.5,
           fmt ("attack change 44.1k vs 96k: %+.1f / %+.1f dB", early[0], early[2]));
    check (std::abs (tail[0] - tail[2]) < 3.0 && std::abs (tail[1] - tail[2]) < 3.0,
           fmt ("tail change 44.1k vs 96k: %+.1f / %+.1f dB", tail[0], tail[2]));
}

void testRobustness()
{
    std::printf ("Extreme settings and nasty input stay finite; silence stays silent\n");
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    bool allFinite = true;
    float maxAbs = 0.0f;

    for (int trial = 0; trial < 40; ++trial)
    {
        Settings s;
        s.attackPercent = uni (rng) * 200.0f - 100.0f;
        s.attackTimeMs = 1.0f + uni (rng) * 199.0f;
        s.releasePercent = uni (rng) * 200.0f - 100.0f;
        s.releaseTimeMs = 10.0f + uni (rng) * 1990.0f;
        s.thresholdDb = -80.0f + uni (rng) * 80.0f;
        s.killPercent = uni (rng) * 100.0f;
        s.holdMs = 5.0f + uni (rng) * 495.0f;
        s.lookaheadMs = uni (rng) * 10.0f;
        s.outputDb = uni (rng) * 48.0f - 24.0f;
        s.mixPercent = uni (rng) * 100.0f;

        std::vector<float> x (24000);
        for (size_t i = 0; i < x.size(); ++i)
        {
            const int kind = (int) (i / 3000) % 4;
            x[i] = kind == 0 ? (uni (rng) * 2.0f - 1.0f) * 4.0f                // loud noise
                 : kind == 1 ? 0.0f                                           // silence
                 : kind == 2 ? 1.0f                                           // DC
                             : (i % 997 == 0 ? 1.0f : 1.0e-38f);              // impulses on denormal floor
        }

        Processor proc;
        proc.prepare (44100.0, 64, 1);
        proc.setSettings (s);
        for (size_t pos = 0; pos < x.size(); pos += 64)
        {
            float* ptr = x.data() + pos;
            proc.process (&ptr, 1, 64);

            if (pos == 6400) // change settings mid-stream
            {
                s.lookaheadMs = 10.0f - s.lookaheadMs;
                s.killPercent = 100.0f - s.killPercent;
                proc.setSettings (s);
            }
        }

        for (float v : x)
        {
            allFinite = allFinite && std::isfinite (v);
            maxAbs = std::max (maxAbs, std::abs (v));
        }
    }

    check (allFinite, "no NaN or Inf");
    check (maxAbs < 4.0f * 15.9f * 16.0f * 16.0f, fmt ("output bounded (peak %.1f)", maxAbs));

    Settings s;
    s.attackPercent = 100.0f;
    s.releasePercent = 100.0f;
    s.killPercent = 50.0f;
    std::vector<float> zeros (48000, 0.0f);
    const auto r = render (zeros, s, sr);
    bool silent = true;
    for (float v : r.output)
        silent = silent && v == 0.0f;
    check (silent, "silence in -> exact silence out");
}

void testMeterFrames()
{
    std::printf ("Meter frames are produced\n");
    Processor proc;
    proc.prepare (sr, 480, 1);
    proc.setSettings (Settings {});
    std::vector<float> x (4800, 0.5f);
    float* ptr = x.data();
    proc.process (&ptr, 1, (int) x.size());

    int frames = 0;
    killroom::MeterFrame f;
    float lastIn = -200.0f;
    while (proc.getMeterFifo().pop (f))
    {
        ++frames;
        lastIn = f.inputDb;
    }
    check (frames == 20, fmt ("100 ms -> 20 frames (%.0f)", frames));
    check (std::abs (lastIn - (-6.02)) < 0.1, fmt ("frame reports input level (%.2f dB)", lastIn));
}
} // namespace

int main()
{
    testTransparentAtDefaults();
    testLookaheadLatency();
    testAttack();
    testRelease();
    testRoomKiller();
    testGhostNotesSurvive();
    testThreshold();
    testMixAndOutput();
    testSampleRateIndependence();
    testRobustness();
    testMeterFrames();

    std::printf (failures == 0 ? "\nAll DSP tests passed\n" : "\n%d DSP check(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

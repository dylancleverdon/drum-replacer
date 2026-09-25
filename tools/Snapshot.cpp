// Development tool: runs some synthetic drums through the plugin, opens the editor
// off-screen and saves it as a PNG. Lets the UI be checked without a DAW (and on Linux
// under xvfb-run). Built with -DKILLROOM_BUILD_SNAPSHOT=ON.

#include <juce_gui_basics/juce_gui_basics.h>

#include <random>

#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;

    const auto output = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png");
    const double sampleRate = 48000.0;
    const int blockSize = 512;

    std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
    auto& processor = dynamic_cast<KillroomAudioProcessor&> (*plugin);

    auto set = [&] (const char* id, float value)
    {
        auto* p = processor.state.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    set (params::attack, 45.0f);
    set (params::release, -30.0f);
    set (params::roomKill, 35.0f);
    set (params::roomHold, 80.0f);
    set (params::threshold, -45.0f);

    plugin->setPlayConfigDetails (2, 2, sampleRate, blockSize);
    plugin->prepareToPlay (sampleRate, blockSize);

    // Hits every 400 ms with a body and a room tail, alternating loud and quiet.
    std::mt19937 rng (3);
    std::normal_distribution<float> noise (0.0f, 1.0f);
    const auto total = (int) (3.3 * sampleRate);
    std::vector<float> drums ((size_t) total, 0.0f);

    for (int hit = 0; hit < 9; ++hit)
    {
        const auto start = (int) ((0.05 + hit * 0.4) * sampleRate);
        const float gain = hit % 2 == 0 ? 0.9f : 0.3f;
        float lp = 0.0f;

        for (int i = start; i < total; ++i)
        {
            const double t = (i - start) / sampleRate;
            const float n = noise (rng);
            lp += 0.3f * (n - lp);
            const double body = std::exp (-t / 0.04);
            const double room = t < 0.004 ? 0.0 : 0.25 * std::exp (-(t - 0.004) / 0.13);
            drums[(size_t) i] += gain * (float) (body * (0.6 * std::sin (2.0 * juce::MathConstants<double>::pi * 180.0 * t) + 0.4 * n)
                                                 + room * lp * 2.0);
        }
    }

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    for (int pos = 0; pos + blockSize <= total; pos += blockSize)
    {
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, drums.data() + pos, blockSize);

        plugin->processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorAndMakeActive());

    // Let the editor's timer drain the meters, and give the update check a moment.
    for (int i = 0; i < 60; ++i)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        if (i >= 3 && processor.updates->getStatus().state != UpdateService::State::checking)
            break;
    }

    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat().writeImageToStream (image, stream);

    std::printf ("update status: %s\n", processor.updates->getStatus().message.toRawUTF8());
    std::printf ("wrote %s\n", output.getFullPathName().toRawUTF8());

    editor.reset();
    plugin.reset();
    return 0;
}

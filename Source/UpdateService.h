#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

// Checks GitHub for a newer Killroom release and installs it in place.
//
// Releases are published by .github/workflows/build.yml. Installing runs the same
// installer script a person would run by hand (scripts/install-mac.sh or
// scripts/install-windows.ps1): it replaces the .vst3 bundle this plugin was loaded
// from, so Live picks up the new version the next time it starts. The copy that is
// already loaded keeps running untouched until then.
class UpdateService : public juce::ChangeBroadcaster,
                      private juce::Thread
{
public:
    enum class State
    {
        idle,
        checking,
        upToDate,
        available,
        noReleases,
        installing,
        installed, // macOS: done, restart Live
        launched,  // Windows: installer window opened
        failed
    };

    struct Status
    {
        State state = State::idle;
        juce::String latestVersion; // e.g. "1.0.5"
        juce::String message;       // progress or error detail
    };

    UpdateService();
    ~UpdateService() override;

    // Asks GitHub for the newest release. Without force it only runs once per host session.
    void checkForUpdates (bool force);

    // Installs the newest release over this plugin's bundle.
    void installUpdate();

    Status getStatus() const;

    static bool canInstall();
    static juce::String currentVersion();
    static int compareVersions (const juce::String& a, const juce::String& b);
    static juce::File findPluginBundle();
    static bool writeScript (const juce::File& file, const juce::String& script);

private:
    enum class Task { none, check, install };

    void run() override;
    void doCheck();
    void doInstall();
    void setStatus (State state, const juce::String& latestVersion, const juce::String& message = {});
    void startTask (Task task);
    juce::String fetch (const juce::String& url, int& statusCode);
    juce::String installerScript (const juce::String& tag);

    mutable juce::CriticalSection lock;
    Status status;
    Task pending = Task::none;
    juce::String latestTag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateService)
};

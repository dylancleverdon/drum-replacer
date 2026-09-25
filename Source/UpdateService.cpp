#include "UpdateService.h"

#include <BinaryData.h>

namespace
{
    const juce::String repo { KILLROOM_GITHUB_REPO };

    // Printed by the installer scripts as their last line when the install worked.
    const juce::String successMarker { "KILLROOM_INSTALL_OK" };

   #if JUCE_MAC
    const char* const installerName = "install-mac.sh";

    juce::String lastLineOf (const juce::File& file)
    {
        auto lines = juce::StringArray::fromLines (file.loadFileAsString());
        lines.removeEmptyStrings();
        return lines.isEmpty() ? juce::String() : lines[lines.size() - 1].trim();
    }
   #else
    const char* const installerName = "install-windows.ps1";
   #endif

    juce::String embeddedInstaller()
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const auto* name = BinaryData::namedResourceList[i];

            if (juce::String (BinaryData::getNamedResourceOriginalFilename (name)) == installerName)
            {
                int size = 0;
                const auto* data = BinaryData::getNamedResource (name, size);
                return juce::String::fromUTF8 (data, size);
            }
        }

        return {};
    }
}

UpdateService::UpdateService() : juce::Thread ("Killroom updater") {}

UpdateService::~UpdateService()
{
    // A running macOS installer keeps going on its own; we just stop watching it.
    signalThreadShouldExit();
    notify();
    stopThread (12000);
}

//==============================================================================
juce::String UpdateService::currentVersion()
{
    return KILLROOM_VERSION_LABEL;
}

int UpdateService::compareVersions (const juce::String& a, const juce::String& b)
{
    // Semver-ish: "1.2.3" > "1.2.3-dev" > "1.2.2"
    auto split = [] (const juce::String& v)
    {
        const auto clean = v.trim().trimCharactersAtStart ("vV");
        const auto core = clean.upToFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("+", false, false);
        const bool prerelease = clean.substring (core.length()).startsWithChar ('-');
        auto parts = juce::StringArray::fromTokens (core, ".", {});

        std::array<int, 3> numbers {};
        for (int i = 0; i < 3 && i < parts.size(); ++i)
            numbers[(size_t) i] = parts[i].getIntValue();

        return std::make_pair (numbers, prerelease);
    };

    const auto [na, pa] = split (a);
    const auto [nb, pb] = split (b);

    for (size_t i = 0; i < 3; ++i)
        if (na[i] != nb[i])
            return na[i] < nb[i] ? -1 : 1;

    if (pa != pb)
        return pa ? -1 : 1;

    return 0;
}

juce::File UpdateService::findPluginBundle()
{
    // macOS:   .../Killroom.vst3/Contents/MacOS/Killroom
    // Windows: ...\Killroom.vst3\Contents\x86_64-win\Killroom.vst3
    const auto binary = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    for (auto dir = binary.getParentDirectory(); dir != dir.getParentDirectory(); dir = dir.getParentDirectory())
        if (dir.hasFileExtension ("vst3") && dir.isDirectory())
            return dir;

    return {};
}

bool UpdateService::canInstall()
{
   #if JUCE_MAC || JUCE_WINDOWS
    return findPluginBundle().isDirectory();
   #else
    return false;
   #endif
}

//==============================================================================
UpdateService::Status UpdateService::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void UpdateService::setStatus (State newState, const juce::String& latestVersion, const juce::String& message)
{
    {
        const juce::ScopedLock sl (lock);
        status = { newState, latestVersion, message };
    }

    sendChangeMessage();
}

void UpdateService::checkForUpdates (bool force)
{
    {
        const juce::ScopedLock sl (lock);

        if (status.state == State::checking || status.state == State::installing)
            return;

        if (! force && status.state != State::idle)
            return;
    }

    startTask (Task::check);
}

void UpdateService::installUpdate()
{
    {
        const juce::ScopedLock sl (lock);

        if (status.state != State::available && status.state != State::failed)
            return;
    }

    startTask (Task::install);
}

void UpdateService::startTask (Task task)
{
    {
        const juce::ScopedLock sl (lock);
        pending = task;
    }

    if (isThreadRunning())
        notify();
    else
        startThread (juce::Thread::Priority::low);
}

void UpdateService::run()
{
    while (! threadShouldExit())
    {
        Task task;

        {
            const juce::ScopedLock sl (lock);
            task = pending;
            pending = Task::none;
        }

        if (task == Task::check)
            doCheck();
        else if (task == Task::install)
            doInstall();
        else
            wait (-1);
    }
}

//==============================================================================
juce::String UpdateService::fetch (const juce::String& url, int& statusCode)
{
    statusCode = 0;

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (10000)
                             .withNumRedirectsToFollow (5)
                             .withStatusCode (&statusCode)
                             .withExtraHeaders ("User-Agent: Killroom/" + currentVersion() + "\r\n"
                                                "Accept: application/vnd.github+json")
                             .withProgressCallback ([this] (int, int) { return ! threadShouldExit(); });

    if (auto stream = juce::URL (url).createInputStream (options))
        return stream->readEntireStreamAsString();

    return {};
}

void UpdateService::doCheck()
{
    setStatus (State::checking, {});

    int code = 0;
    const auto body = fetch ("https://api.github.com/repos/" + repo + "/releases/latest", code);

    if (threadShouldExit())
        return;

    if (code == 404)
    {
        setStatus (State::noReleases, {});
        return;
    }

    const auto tag = juce::JSON::parse (body).getProperty ("tag_name", {}).toString();

    if (code != 200 || tag.isEmpty())
    {
        setStatus (State::failed, {}, code == 0 ? "Couldn't reach GitHub" : "GitHub answered " + juce::String (code));
        return;
    }

    const auto latest = tag.trimCharactersAtStart ("vV");

    {
        const juce::ScopedLock sl (lock);
        latestTag = tag;
    }

    setStatus (compareVersions (latest, currentVersion()) > 0 ? State::available : State::upToDate, latest);
}

bool UpdateService::writeScript (const juce::File& file, const juce::String& script)
{
    // Byte for byte: File::replaceWithText would turn line endings into CRLF, which breaks bash.
    return file.replaceWithData (script.toRawUTF8(), script.getNumBytesAsUTF8());
}

juce::String UpdateService::installerScript (const juce::String& tag)
{
    // Prefer the installer from the release being installed, so fixes to the installer
    // reach people running older versions. Fall back to the copy built into this plugin.
    int code = 0;
    const auto remote = fetch ("https://raw.githubusercontent.com/" + repo + "/" + tag + "/scripts/" + installerName, code);

    if (code == 200 && remote.contains (successMarker))
        return remote;

    return embeddedInstaller();
}

void UpdateService::doInstall()
{
    juce::String tag, latest;

    {
        const juce::ScopedLock sl (lock);
        tag = latestTag;
        latest = status.latestVersion;
    }

   #if JUCE_MAC || JUCE_WINDOWS
    const auto bundle = findPluginBundle();

    if (! bundle.isDirectory())
    {
        setStatus (State::failed, latest, "Only the installed VST3 can update itself");
        return;
    }

    if (tag.isEmpty())
    {
        setStatus (State::failed, latest, "Check for updates first");
        return;
    }

    setStatus (State::installing, latest, "Downloading installer");

    const auto script = installerScript (tag);
    const auto workDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("KillroomUpdate");
    workDir.createDirectory();
    const auto scriptFile = workDir.getChildFile (installerName);

    if (script.isEmpty() || ! writeScript (scriptFile, script))
    {
        setStatus (State::failed, latest, "Couldn't prepare the installer");
        return;
    }

    const auto destination = bundle.getParentDirectory().getFullPathName();

   #if JUCE_MAC
    const auto logFile = workDir.getChildFile ("update.log");
    logFile.deleteFile();

    juce::ChildProcess process;
    const juce::StringArray command { "/bin/bash", scriptFile.getFullPathName(),
                                      "--dest", destination,
                                      "--tag", tag,
                                      "--log", logFile.getFullPathName() };

    if (! process.start (command, 0))
    {
        setStatus (State::failed, latest, "Couldn't start the installer");
        return;
    }

    juce::String shown;

    while (process.isRunning())
    {
        if (threadShouldExit())
            return; // the installer finishes by itself

        const auto line = lastLineOf (logFile);

        if (line.isNotEmpty() && line != shown)
        {
            shown = line;
            setStatus (State::installing, latest, line);
        }

        wait (250);
    }

    const auto result = lastLineOf (logFile);

    if (result.startsWith (successMarker))
        setStatus (State::installed, latest);
    else
        setStatus (State::failed, latest, result.isNotEmpty() ? result : "The installer stopped unexpectedly");
   #else
    // Installing into Program Files needs admin rights, so the script asks for them (UAC)
    // in its own window and reports progress there.
    const auto powershell = juce::File (juce::SystemStats::getEnvironmentVariable ("SystemRoot", "C:\\Windows"))
                                .getChildFile ("System32\\WindowsPowerShell\\v1.0\\powershell.exe");

    const auto arguments = "-NoProfile -ExecutionPolicy Bypass -File \"" + scriptFile.getFullPathName()
                         + "\" -Dest \"" + destination + "\" -Tag " + tag;

    if (powershell.startAsProcess (arguments))
        setStatus (State::launched, latest);
    else
        setStatus (State::failed, latest, "Couldn't start PowerShell");
   #endif
   #else
    setStatus (State::failed, latest, "Automatic install isn't available on this platform");
   #endif
}

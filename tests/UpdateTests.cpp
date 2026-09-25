// Tests for the parts of the updater that decide whether to offer an update.
// Built with KILLROOM_VERSION_LABEL="1.0.5" (see CMakeLists.txt).

#include <BinaryData.h>

#include "../Source/UpdateService.h"

#include <cstdio>
#include <cstring>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::printf ("  [%s] %s\n", condition ? "ok" : "FAIL", what.toRawUTF8());
    if (! condition)
        ++failures;
}

void expectOrder (const juce::String& older, const juce::String& newer)
{
    check (UpdateService::compareVersions (older, newer) < 0 && UpdateService::compareVersions (newer, older) > 0,
           older + " < " + newer);
}

juce::String resource (const char* originalName)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const auto* name = BinaryData::namedResourceList[i];

        if (juce::String (BinaryData::getNamedResourceOriginalFilename (name)) == originalName)
        {
            int size = 0;
            const auto* data = BinaryData::getNamedResource (name, size);
            return juce::String::fromUTF8 (data, size);
        }
    }

    return {};
}
}

int main()
{
    std::printf ("Version comparison\n");
    expectOrder ("1.0.4", "1.0.5");
    expectOrder ("1.0.9", "1.0.10");
    expectOrder ("1.0.99", "1.1.0");
    expectOrder ("1.9.9", "2.0.0");
    expectOrder ("1.0.5-dev+abc1234", "1.0.5");
    expectOrder ("1.0.5", "1.0.6-dev+abc1234");
    check (UpdateService::compareVersions ("v1.0.5", "1.0.5") == 0, "v1.0.5 == 1.0.5");
    check (UpdateService::compareVersions ("1.0.5-dev+aaa", "1.0.5-dev+bbb") == 0, "dev builds of the same version are equal");

    std::printf ("Current version\n");
    check (UpdateService::currentVersion() == "1.0.5", "reports the compiled-in version");
    check (UpdateService::compareVersions ("v1.0.6", UpdateService::currentVersion()) > 0, "a newer release is offered");
    check (UpdateService::compareVersions ("v1.0.5", UpdateService::currentVersion()) == 0, "the same release is not offered");

    std::printf ("Embedded installers\n");
    const auto mac = resource ("install-mac.sh");
    const auto windows = resource ("install-windows.ps1");
    check (mac.startsWith ("#!/bin/bash") && mac.contains ("KILLROOM_INSTALL_OK"), "macOS installer is embedded");
    check (windows.contains ("param(") && windows.contains ("KILLROOM_INSTALL_OK"), "Windows installer is embedded");
    check (! mac.containsChar ('\r'), "embedded macOS installer has LF line endings");

    std::printf ("Writing the installer to disk\n");
    const auto file = juce::File::createTempFile (".sh");
    check (UpdateService::writeScript (file, mac), "script written");
    juce::MemoryBlock written;
    file.loadFileAsData (written);
    check (written.getSize() == mac.getNumBytesAsUTF8()
               && std::memcmp (written.getData(), mac.toRawUTF8(), written.getSize()) == 0,
           "written byte for byte (no CRLF conversion)");
    file.deleteFile();

    std::printf (failures == 0 ? "\nAll update tests passed\n" : "\n%d update check(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

# Killroom: notes for working on this repo

Killroom is a JUCE VST3 transient shaper and room killer for Ableton Live (macOS and Windows).
The owner uses it in Live and gets changes through the plugin's built-in updater, so every
merge to the default branch ships to them. Keep it working and keep saved Live sets loading.

## Layout

- `Source/dsp/Killroom.{h,cpp}`: all the audio processing. Plain C++ with no JUCE, so it can be unit tested. The header comment describes the signal flow.
- `Source/Parameters.{h,cpp}`: parameter IDs, ranges, defaults and value text.
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`, `Source/Ui.*`: the JUCE wrapper, editor, look and feel, and scrolling display.
- `Source/UpdateService.*`: checks GitHub releases and runs the installer script to update in place.
- `scripts/install-mac.sh`, `scripts/install-windows.ps1`: install/update scripts. They're embedded in the plugin as BinaryData and also run by users directly.
- `tests/DspTests.cpp` (behaviour of the DSP), `tests/UpdateTests.cpp` (version comparison, embedded installers), `tests/test-installer-*` (end-to-end installer tests, run in CI on real macOS and Windows).
- `tools/Snapshot.cpp`: renders the editor to a PNG so UI changes can be checked without a DAW.
- `.github/workflows/build.yml`: builds, tests, validates with pluginval, and releases.

## Things that must not change

Changing any of these breaks saved Live sets or the updater for people already running Killroom:

- `PLUGIN_MANUFACTURER_CODE Dcln`, `PLUGIN_CODE Kilr`, `PRODUCT_NAME "Killroom"` and `BUNDLE_ID` in `CMakeLists.txt`.
- Parameter IDs in `Source/Parameters.h`. Don't rename or remove them. To add a parameter, give it a new ID and a version hint above 1. Changing a parameter's range changes what saved automation means.
- The state format: `setStateInformation` has to keep loading states saved by older versions.
- Release asset names `Killroom-macOS.zip` and `Killroom-Windows.zip` (each containing `Killroom.vst3` at the top level), the `vX.Y.Z` tag format, and the `KILLROOM_INSTALL_OK` last line the installers print. Older plugins in the wild depend on all of these.
- The installer CLI flags (`--dest --tag --log` and `-Dest -Tag`), which older plugins pass.

## Build and test (Linux works fine for development)

```sh
# One-time Linux deps: libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev libcurl4-openssl-dev xvfb
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DKILLROOM_BUILD_SNAPSHOT=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Look at the UI
xvfb-run -a build/KillroomSnapshot_artefacts/Release/KillroomSnapshot /tmp/killroom.png

# Validate the plugin (pluginval_Linux.zip from github.com/Tracktion/pluginval/releases)
xvfb-run -a ./pluginval --strictness-level 10 --validate-in-process --validate build/Killroom_artefacts/Release/VST3/Killroom.vst3
```

To iterate on the DSP quickly without JUCE: `g++ -std=c++17 -O2 tests/DspTests.cpp Source/dsp/Killroom.cpp && ./a.out`.

When you change DSP behaviour, update or add checks in `tests/DspTests.cpp`. Those tests measure the effect on specific time windows of a synthetic drum track.

## Releasing

A push to the default branch makes CI publish release `v<major.minor from VERSION>.<next patch>`. The
release notes are the commit subjects since the last tag, so write commit subjects the owner can read
as a changelog. Bump `VERSION` for a new minor or major line. Other branches and PRs build, test and
upload artifacts without releasing.

## Update mechanics (why it's built this way)

- macOS installs to `~/Library/Audio/Plug-Ins/VST3`, so no admin rights are needed. The installer moves the old bundle aside instead of overwriting it, because rewriting a loaded binary in place can crash Live. The plugin runs the script with `/bin/bash` in the background and shows progress from `--log`.
- Windows installs to `Program Files\Common Files\VST3`, which needs admin rights. The plugin opens the script in a PowerShell window, and the script asks for elevation itself. A loaded DLL can't be overwritten but can be renamed, so the installer renames it to `*.old-<timestamp>` and cleans those up on the next run.
- The plugin fetches the installer from the tag it's installing, and falls back to its embedded copy. That way installer fixes also reach people running older versions.

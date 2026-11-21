# GRANULAR_SYSTEM — plugin

A granular sampler and synth with a marker-based step sequencer, ported from the web version. It builds as:

| Format | For |
|---|---|
| **AU** (macOS) | Logic Pro, GarageBand, MainStage, plus most Mac DAWs |
| **VST3** (macOS / Windows) | Ableton Live, FL Studio, Cubase, Studio One, Reaper, Bitwig, … |
| **Standalone app** | Use it without a DAW |

Logic only loads AU plugins, not VST3.

---

## Option A: let GitHub build it for you (no developer tools needed)

1. Make a free GitHub account, create a new **private** repository, and upload everything in this folder (keep the `.github` folder).
2. Open the repository's **Actions** tab. The *Build plugin* workflow runs automatically, taking about 15 minutes.
3. When it finishes, open the run and download **GRANULAR_SYSTEM-macOS** and/or **GRANULAR_SYSTEM-Windows** from *Artifacts*.
4. Install the downloaded files as shown in "Installing" below.

The macOS build isn't signed with an Apple Developer certificate. The first time, macOS may block it: run this in Terminal (adjust the path if needed), then rescan in your DAW.

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/GRANULAR_SYSTEM.component ~/Library/Audio/Plug-Ins/VST3/GRANULAR_SYSTEM.vst3
```

## Option B: build it yourself

### macOS (for Logic)
1. Install **Xcode** from the App Store and open it once to accept the licence.
2. Install CMake: download it from cmake.org, or run `brew install cmake` if you use Homebrew.
3. In Terminal, from this folder:
   ```
   cmake -B build -G Xcode
   cmake --build build --config Release
   ```
   The first build downloads JUCE (the framework) and takes 5–10 minutes.
4. The AU and VST3 are copied into `~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3` automatically.

### Windows
1. Install **Visual Studio 2022 Community** with the "Desktop development with C++" workload, and **CMake**.
2. In *Developer PowerShell for VS 2022*, from this folder:
   ```
   cmake -B build
   cmake --build build --config Release
   ```
3. Copy `build\GranularSystem_artefacts\Release\VST3\GRANULAR_SYSTEM.vst3` into `C:\Program Files\Common Files\VST3\`.

## Installing (downloaded builds)
- **Mac AU:** `GRANULAR_SYSTEM.component` goes in `~/Library/Audio/Plug-Ins/Components/`
- **Mac VST3:** `GRANULAR_SYSTEM.vst3` goes in `~/Library/Audio/Plug-Ins/VST3/`
- **Windows VST3:** `GRANULAR_SYSTEM.vst3` goes in `C:\Program Files\Common Files\VST3\`

### Using it in Logic Pro
Create a **Software Instrument** track and choose **AU Instruments → GRANULAR_SYSTEM → GRANULAR_SYSTEM**. If it doesn't appear, open *Logic Pro → Settings → Plug-in Manager*, select it, and click **Reset & Rescan Selection**.

---

## MIDI
- **Notes:** in KEYS **PLAY** mode every note plays its own grain cloud from the position line, pitched relative to middle C (MIDI 60), with velocity setting the level. In **TRANSPOSE SEQ** mode, notes transpose the running sequence.
- **Sustain pedal (CC64)** and the **HOLD** button latch notes.
- **Pitch bend:** the range (0–24 semitones) is set by the BEND RANGE knob.
- **Mod wheel (CC1):** pushes any parameter you choose in the "MOD WHEEL →" menu.
- **MIDI Learn:** right-click any knob, choose *MIDI LEARN*, then move a controller. Mappings are saved with your project.
- **Sequencer sync:** locks to the DAW's tempo, bar position and loop when the transport is playing. With the transport stopped, **RUN** plays it from the internal TEMPO.
- **Automation:** every parameter can be automated in the DAW.

## Project files
- `Source/Engine.h`: the sound engine (no framework code; tested by `tests/engine_test.cpp`)
- `Source/Params.h`, `Presets.h`, `Samples.h`, `Format.h`: parameters, presets, demo sounds, hit detection
- `Source/PluginProcessor.*`: MIDI, host sync, sample loading, saved state, MIDI learn
- `Source/PluginEditor.*`: the interface
- `tests/`: offline engine test and a headless host test

Sample files are stored by path. If you move a sample, reload it in the plugin.

JUCE is free for personal use under its AGPLv3 / starter licence terms. If you ever sell the plugin, check JUCE's licensing.

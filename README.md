# ZavTune — pitch correction VST3 for FL Studio

An Auto-Tune-style vocal pitch corrector. It finds the pitch of your voice and snaps it to the nearest note in the key and scale you pick. With Retune Speed at 0 you get the hard, robotic "T-Pain" sound. Turn it up for natural, transparent correction.

## Controls

| Control | What it does |
|---|---|
| **Key / Scale** | The notes it is allowed to snap to (Chromatic, Major, Minor, Major/Minor Pentatonic, Blues, Harmonic Minor) |
| **Retune Speed** | 0 ms = instant hard-tune. 20–50 ms = modern pop. 100+ ms = subtle |
| **Amount** | How much correction is applied (100% = full snap) |
| **Humanize** | Relaxes correction on long held notes so they keep natural vibrato |
| **Transpose** | Shifts everything up/down in semitones (±12) |
| **Mix** | Dry/wet blend |

The display shows the target note, the detected frequency, and how many cents it's correcting.

## Getting the .vst3 file (easiest: no installs needed)

1. Create a new repository on GitHub and upload this whole folder (including the hidden `.github` folder).
2. Open the repo's **Actions** tab. The "Build ZavTune" workflow runs on its own (or click **Run workflow**).
3. When it finishes (~5–10 min), open the run and download **ZavTune-Windows-VST3**.
4. Unzip it and copy the `ZavTune.vst3` folder to `C:\Program Files\Common Files\VST3\`.

## Building on your own PC (alternative)

You need Visual Studio 2022 (with "Desktop development with C++") and CMake. In this folder:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ZavTune_VST3
```

Output: `build\ZavTune_artefacts\Release\VST3\ZavTune.vst3`. Copy it to `C:\Program Files\Common Files\VST3\`.

## Using it in FL Studio

1. **Options → Manage plugins → Find installed plugins**, then add ZavTune to your favorites.
2. Put it on the mixer insert your vocal mic or vocal track goes to.
3. Set **Key** and **Scale** to match the beat (if you're unsure, Chromatic always works but sounds less "musical").
4. For the hard sound: Retune 0, Amount 100%. For a cleaner sound: Retune 30–60 ms, Humanize 30–50%.

Tips:
- Put ZavTune **before** reverb/delay, and after any noise gate or EQ.
- Feed it a dry, single voice. It's monophonic, so harmonies or a backing track mixed in will confuse it.
- It adds about 15 ms of latency. FL compensates on playback; for live monitoring, use a small ASIO buffer.

## How it works

- **Detection:** YIN pitch tracking (70–1000 Hz) on a mono sum, every ~5 ms, with a noise gate and note hysteresis so it doesn't flutter between notes.
- **Correction:** the target note's offset is smoothed by Retune Speed, then scaled by Amount.
- **Shifting:** a two-grain crossfaded delay-line pitch shifter with cubic interpolation (low CPU, low latency).

The DSP lives in `Source/ZavTuneDSP.h` with no JUCE dependency, so it's easy to tweak.

Built with JUCE 8 (downloaded automatically during the build). JUCE is free for personal use and small earners. See juce.com for its licensing terms.

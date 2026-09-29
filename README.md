# SustainTail

A cross-platform MIDI effect plugin (VST3 / AU) that gives every note an adjustable release tail by delaying its note-off — sustain-pedal feel without a pedal.

## What it does

Play a note and let go, and instead of stopping immediately the note keeps ringing
for a fixed amount of time, then stops. It does this by holding back each MIDI
**note-off** message by an adjustable delay (the "Release tail"), so the instrument
sustains and decays naturally — exactly like holding a sustain pedal for a fixed
duration. It is **not** an echo/delay effect: there are no repeats, just one longer note.

Because it works purely on MIDI, it works with any instrument (piano, synth, pads, etc.).

## Controls

- **Release tail (ms)** — how long each note keeps sounding after you release the key
  (0–10000 ms, default 2000 ms).

If you replay a note while it is still ringing on its delayed release, the old one is
cut cleanly and the note retriggers.

## Formats

- **VST3** — Windows and macOS (Reaper, Ableton, Cubase, Studio One, Bitwig, FL, …)
- **AU** — macOS only (Logic Pro, GarageBand)

## Usage in a DAW

Add SustainTail to your instrument track **above** the instrument in the FX/MIDI chain
(MIDI flows top → bottom, so the sustain processor must come first), then adjust the
Release tail slider.

## Building from source (macOS)

Requirements: Xcode Command Line Tools, CMake. JUCE is fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

The build produces universal (arm64 + x86_64) binaries and installs them to your user
plugin folders (`~/Library/Audio/Plug-Ins/VST3` and `.../Components`).

### Windows

The same source compiles on Windows to produce a Windows `.vst3` (build with CMake +
Visual Studio, or via CI such as GitHub Actions). AU is macOS-only and not built on Windows.

## License

MIT — see [LICENSE](LICENSE).

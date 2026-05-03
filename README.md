# DEATH DEALER DRUMS
### by INFERNO TONES

The most brutal sample-based drum VSTi ever made.  
Runs on **Windows** (VST3) and **macOS** (VST3 + AU).

---

## Features

| Feature | Details |
|---|---|
| **12 pads** | Kick, Snare, Rimshot, HH Closed/Pedal/Open, Tom ×3, Crash, Ride, China |
| **Voice engine** | 64-voice polyphonic pool, velocity layers, round-robin, pitch interpolation |
| **Choke groups** | Hi-hat family chokes on new hit; configurable |
| **Per-pad EQ** | 3-band (low shelf 100 Hz, peak 1 kHz, high shelf 8 kHz) |
| **Per-pad comp** | Feed-forward RMS compressor with threshold + ratio |
| **Per-pad saturation** | Tanh soft clipper with variable drive |
| **Transient shaper** | Attack boost envelope per pad |
| **Tune** | ±24 semitones pitch shift with linear interpolation |
| **Decay** | Variable envelope shortening (natural → 30 ms staccato) |
| **Reverb send** | Per-pad send to shared Freeverb room reverb |
| **Global reverb** | Size, Damping, Mix controls |
| **Master limiter** | Brick-wall with 1 ms attack / 120 ms release |
| **Preset save/restore** | Full state + sample file paths persisted via DAW |
| **Sample loading** | Load any WAV/AIFF/FLAC per pad at runtime |
| **Built-in sounds** | Synthetic default sounds on every pad (no samples required) |
| **GM MIDI mapping** | Default standard kit map; each note fully reassignable |

---

## Build Requirements

### Windows
- Visual Studio 2022 (with C++ desktop workload)
- CMake 3.22+
- Git
- Internet connection (JUCE 8.0.5 fetched automatically)

```bat
scripts\build-windows.bat
```

### macOS
- Xcode 14+ (Command Line Tools)
- CMake 3.22+
- Git

```bash
chmod +x scripts/build-mac.sh
scripts/build-mac.sh
```

macOS builds a **Universal Binary** (arm64 + x86_64) targeting macOS 11.0+.

---

## Project Structure

```
DEATH DEALER DRUMS/
├── CMakeLists.txt          Main CMake build file
├── source/
│   ├── DrumPad.h/.cpp      Sample storage (velocity layers, round-robin)
│   ├── DrumEngine.h/.cpp   Real-time voice engine + DSP
│   ├── PluginProcessor.h/.cpp  JUCE plugin processor + APVTS
│   └── PluginEditor.h/.cpp     Dark metal UI
├── scripts/
│   ├── build-windows.bat
│   └── build-mac.sh
└── DELIVERABLES/           Build output (auto-created)
```

---

## Default MIDI Map (GM Standard)

| Pad | MIDI Note |
|---|---|
| Kick | 36 (C2) |
| Snare | 38 (D2) |
| Rimshot | 37 (C#2) |
| HH Closed | 42 (F#2) |
| HH Pedal | 44 (G#2) |
| HH Open | 46 (A#2) |
| Tom 1 (floor) | 41 (F2) |
| Tom 2 (mid) | 45 (A2) |
| Tom 3 (high) | 48 (C3) |
| Crash | 49 (C#3) |
| Ride | 51 (D#3) |
| China | 52 (E3) |

All notes are reassignable from the plugin UI.

---

## Loading Samples

Click **LOAD** on any channel strip, or **LOAD SAMPLE** in the pad detail panel.  
Supported formats: WAV, AIFF, FLAC (any bit depth; auto-converted to 32-bit float stereo).

> For **velocity layers**: load multiple files per pad via future multi-import (planned v1.1).

---

## INFERNO TONES Plugin Family

| Plugin | Type |
|---|---|
| HEXSTACK | Guitar amp sim |
| SUBJUGATOR | Amp/effects pedal |
| **DEATH DEALER DRUMS** | **Drum VSTi** |

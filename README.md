# chordgen

MIDI chord trainer written in [Anchor](https://github.com/allenj12/anchor).

> **Windows only.** The console setup (`SetConsoleOutputCP`, `_setmode`) and build toolchain rely on Windows APIs and MSVC.

## Prerequisites

- [Anchor compiler](https://github.com/allenj12/anchor) (`anchorc` on PATH)
- Visual Studio with the **x64 Native Tools** environment (Community or Build Tools)
- [vcpkg](https://vcpkg.io/) with PortMidi installed:
  ```powershell
  vcpkg install portmidi:x64-windows
  ```
  Add `C:\vcpkg\installed\x64-windows\bin` to your `PATH` so `portmidi.dll` is found at runtime.

## Build

```powershell
.\build.ps1
```

## Usage

```powershell
.\chordgen.exe [options]
```

Running without arguments shows a menu to pick a lesson. Each lesson is a sequence of exercises; each exercise picks a random key and random inversions, then asks you to play through a chord progression repeatedly.

- **Correct** — moves to the next chord in the progression
- **Wrong** — shows what you actually played and lets you try again
- **Unrecognized** — chord wasn't a known major, minor, or diminished triad
- After completing all chords in the progression, the same key and inversions repeat for the remaining rounds, then the next exercise begins

### Options

| Flag | Description |
|------|-------------|
| `-l`, `--list` | List all available lessons and exit. |
| `--lesson N` | Start lesson N directly, skipping the menu. |
| `-s`, `--select-device` | List available MIDI input devices and choose one interactively. Without this flag the first input device is used automatically. |
| `-d`, `--debug` | Print raw MIDI note numbers on every key press and release. |
| `-h`, `--help` | Show usage information and exit. |

### Examples

```powershell
.\chordgen.exe                       # show lesson menu
.\chordgen.exe --list                # list all lessons
.\chordgen.exe --lesson 2            # start lesson 2 directly
.\chordgen.exe -s                    # pick MIDI device interactively
.\chordgen.exe --lesson 3 -d         # lesson 3 with debug note output
```

## Lessons

| # | Name | Description |
|---|------|-------------|
| 1 | C major Triads, infinite |
| 2 | D major Triads, infinite |
| 3 | F major Triads, infinite |
| 4 | B♭ major Triads, infinite |
| 5 | I - V - VI - IV (Major) | I - V - VI - IV in C, D, F, B♭ — 10 reps |

## Chord coverage

Chords the trainer can generate and recognize:

| Type | Chords |
|------|--------|
| Major | C, D, E♭, F, F♯, G, A, B♭ |
| Minor | C, C♯, D, E, G, A, B |
| Diminished | E, A, B |

All three inversions (root, 1st, 2nd) are included for each chord.

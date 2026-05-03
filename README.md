# chordgen

MIDI chord trainer written in [Anchor](https://github.com/allenj12/anchor).

> **Windows only.** The console setup (`SetConsoleOutputCP`, `_setmode`) and build toolchain rely on Windows APIs and MSYS2/MinGW.

## Prerequisites

- [Anchor compiler](https://github.com/allenj12/anchor) (`anchorc` on PATH)
- [MSYS2](https://www.msys2.org/) with PortMidi installed:
  ```bash
  pacman -S mingw-w64-x86_64-portmidi
  ```

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
.\chordgen.exe                        # show lesson menu
.\chordgen.exe --list                 # list all lessons
.\chordgen.exe --lesson 2            # start lesson 2 directly
.\chordgen.exe -s                     # pick MIDI device interactively
.\chordgen.exe --lesson 3 -d         # lesson 3 with debug note output
```

## Lessons

| # | Name | Description |
|---|------|-------------|
| 1 | Free Play | All 12 major keys, I only, infinite |
| 2 | I - IV - V (Major) | I - IV - V progression, sharp and flat key groups |
| 3 | I - V - VI - IV (Major) | I - V - VI - IV progression, sharp and flat key groups |
| 4 | Minor Keys | i - iv - v and i - v - VI - VII in natural minor |

## Chord coverage

Chords the trainer can generate and recognize:

| Type | Chords |
|------|--------|
| Major | C, D, E♭, F, F♯, G, A, B♭ |
| Minor | C, C♯, D, E, G, A, B |
| Diminished | E, A, B |

All three inversions (root, 1st, 2nd) are included for each chord.

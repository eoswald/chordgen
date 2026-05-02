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

The program picks a random chord (name and inversion) and waits for you to play it on your MIDI keyboard. Release all keys when you're done:

- **Correct** — moves on to the next chord
- **Wrong** — shows what you actually played and lets you try again
- **Unrecognized** — chord wasn't a known major, minor, or diminished triad

### Options

| Flag | Description |
|------|-------------|
| `-s`, `--select-device` | List available MIDI input devices and choose one interactively. Without this flag the first input device is used automatically. |
| `-d`, `--debug` | Print raw MIDI note numbers on every key press and release. |
| `-h`, `--help` | Show usage information and exit. |

### Examples

```powershell
.\chordgen.exe                        # auto-select MIDI device, start immediately
.\chordgen.exe -s                     # pick MIDI device interactively
.\chordgen.exe -s -d                  # device selection + debug note output
```

## Chord coverage

Chords the trainer can generate and recognize:

| Type | Chords |
|------|--------|
| Major | C, D, E♭, F, F♯, G, A, B♭ |
| Minor | C, C♯, D, E, G, A, B |
| Diminished | E, A, B |

All three inversions (root, 1st, 2nd) are included for each chord.

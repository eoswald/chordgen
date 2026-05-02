# chordgen

MIDI chord trainer written in [Anchor](https://github.com/allenj12/anchor).

## Prerequisites

- [Anchor compiler](https://github.com/allenj12/anchor) (`anchorc` on PATH)
- [MSYS2](https://www.msys2.org/) with portmidi installed:
  ```bash
  pacman -S mingw-w64-x86_64-portmidi
  ```

## Programs

### chordgen

Listens on a MIDI input device. Press Enter to generate a random chord name and inversion. Press Middle C (note 60) on your MIDI controller to confirm you played it for now.

## Build

```powershell
.\build.ps1
```

## Run

```powershell
.\chordgen.exe
```

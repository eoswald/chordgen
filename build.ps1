param([string]$target = "chordgen")
$flags = "-IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lportmidi -lwinmm -Wl,--stack,16777216"
switch ($target) {
    "chordgen" { anchorc chordgen.anc -o chordgen --cflags $flags }
    default    { Write-Error "Unknown target: $target" }
}
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed (exit $LASTEXITCODE)" }

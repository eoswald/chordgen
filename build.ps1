param([string]$target = "chordgen")

# Ensure x64 cl.exe is on PATH by sourcing vcvars64.bat if needed.
# Skip if already in an x64 Native Tools environment (VSCMD_ARG_TGT_ARCH=x64).
if ($env:VSCMD_ARG_TGT_ARCH -ne "x64") {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath 2>$null
        $vcvars64 = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvars64) {
            $envDump = cmd /c "`"$vcvars64`" > nul 2>&1 && set"
            foreach ($line in $envDump) {
                if ($line -match "^([^=]+)=(.*)$") {
                    [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
                }
            }
        }
    }
}

$vcpkg = "C:\vcpkg\installed\x64-windows"
$flags = "/I`"$vcpkg\include`" `"$vcpkg\lib\portmidi.lib`" winmm.lib /link /STACK:16777216"
$src = (Resolve-Path "chordgen.anc").Path
switch ($target) {
    "chordgen" { anchorc $src -o chordgen --cflags $flags }
    default    { Write-Error "Unknown target: $target" }
}
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed (exit $LASTEXITCODE)" }

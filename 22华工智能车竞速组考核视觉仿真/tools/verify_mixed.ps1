# =============================================================================
#  verify_mixed.ps1 -- prove the project really compiles C and C++ together
# -----------------------------------------------------------------------------
#  Checks performed:
#    1. MinGW : image_init / image_process in simu_env.o have UNDECORATED names
#               (C linkage). C++ would produce _Z13image_processv.
#    2. MSVC  : same symbols are undecorated (C++ would be ?image_process@@YAHXZ)
#    3. Negative control: feeding a C++-only construct through the C compiler
#               must FAIL -- this proves the .c files are not being passed to
#               the C++ compiler behind our back.
#
#  USAGE: powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify_mixed.ps1
# =============================================================================

$ErrorActionPreference = 'Continue'

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$pass = 0
$fail = 0
function Ok($m)   { Write-Host "  [PASS] $m" -ForegroundColor Green; $script:pass++ }
function Bad($m)  { Write-Host "  [FAIL] $m" -ForegroundColor Red;   $script:fail++ }
function Info($m) { Write-Host "  [info] $m" -ForegroundColor Gray }

Write-Host ""
Write-Host "=== 1. MinGW: C linkage check (undecorated symbols) ===" -ForegroundColor Cyan

$mingwBin = $null
foreach ($p in @(
    'D:\TOOLS\Dev-Cpp\MinGW64\bin',
    'C:\Program Files (x86)\Dev-Cpp\MinGW64\bin',
    'C:\Program Files\Dev-Cpp\MinGW64\bin',
    'D:\TOOLS\mingw-w64\bin')) {
    if (Test-Path (Join-Path $p 'g++.exe')) { $mingwBin = $p; break }
}
if (-not $mingwBin -and $env:MINGW_HOME) {
    $p = Join-Path $env:MINGW_HOME 'bin'
    if (Test-Path (Join-Path $p 'g++.exe')) { $mingwBin = $p }
}

if (-not $mingwBin) {
    Info "MinGW not found - skipping MinGW checks"
} else {
    Info "compiler: $mingwBin"
    $nm = Join-Path $mingwBin 'nm.exe'

    $obj = Join-Path $Root 'build\simu_env.o'
    if (-not (Test-Path $obj)) {
        Info "build\simu_env.o missing - run tools\build_dev.bat first"
    } else {
        $syms = & $nm $obj 2>$null
        foreach ($fn in @('image_init', 'image_process', 'otsu_get_threshold')) {
            $found = $syms | Where-Object { $_ -match "\sT\s+$fn$" }
            $mangled = $syms | Where-Object { $_ -match "_Z\d+$fn" }
            if ($found -and -not $mangled) { Ok "$fn : C linkage (undecorated)" }
            else { Bad "$fn : NOT C linkage (mangled symbol found)" }
        }
    }
}

Write-Host ""
Write-Host "=== 2. MSVC: C linkage check ===" -ForegroundColor Cyan

# Locate dumpbin through the same version-agnostic path the build scripts use.
# Hard-coding 'C:\Program Files\Microsoft Visual Studio\2022' (as an earlier
# version did) silently skipped this whole section on any other VS version or
# install drive, while still reporting "all checks passed" -- which made the
# script look like it had verified MSVC when it had not.
$dumpbin = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                        -property installationPath 2>$null
    if ($vsRoot) {
        $vsRoot = $vsRoot.Trim()
        $dumpbin = Get-ChildItem (Join-Path $vsRoot 'VC\Tools\MSVC') -Filter 'dumpbin.exe' -Recurse -ErrorAction SilentlyContinue |
                   Where-Object { $_.FullName -match 'Hostx64\\x64' } |
                   Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
}
if (-not $dumpbin) {
    # fallback: any VS install on any drive
    foreach ($base in @("$env:ProgramFiles\Microsoft Visual Studio",
                        "${env:ProgramFiles(x86)}\Microsoft Visual Studio",
                        'D:\Program Files\Microsoft Visual Studio')) {
        if (-not (Test-Path $base)) { continue }
        $dumpbin = Get-ChildItem $base -Filter 'dumpbin.exe' -Recurse -ErrorAction SilentlyContinue |
                   Where-Object { $_.FullName -match 'Hostx64\\x64' } |
                   Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
        if ($dumpbin) { break }
    }
}

$vsObj = Join-Path $Root 'VS\visual_simu\x64\Debug\simu_env.obj'
if (-not (Test-Path $vsObj)) { $vsObj = Join-Path $Root 'VS\x64\Debug\simu_env.obj' }

$msvcChecked = $false
if (-not $dumpbin) {
    Info "dumpbin not found - SKIPPING MSVC checks (this is NOT a pass)"
} elseif (-not (Test-Path $vsObj)) {
    Info "MSVC object missing - SKIPPING MSVC checks (run tools\build_vs.bat first)"
} else {
    Info "dumpbin: $dumpbin"
    Info "object : $vsObj"
    $out = & $dumpbin /SYMBOLS $vsObj 2>$null
    foreach ($fn in @('image_init', 'image_process', 'otsu_get_threshold')) {
        $cStyle = $out | Where-Object { $_ -match "\|\s+$fn\s*$" }
        $cppStyle = $out | Where-Object { $_ -match "\?${fn}@@" }
        if ($cStyle -and -not $cppStyle) { Ok "$fn : C linkage (plain symbol)" }
        else { Bad "$fn : NOT C linkage (MSVC C++ mangling found)" }
    }
    $msvcChecked = $true
}

Write-Host ""
Write-Host "=== 3. Negative control: C++-only syntax must NOT compile as C ===" -ForegroundColor Cyan

if (-not $mingwBin) {
    Info "MinGW not found - skipping negative control"
} else {
    $gcc = Join-Path $mingwBin 'gcc.exe'
    $tmp = Join-Path $env:TEMP '_mixed_probe.c'

    # `namespace` is a C++ keyword; a C compiler must reject it.
    @'
int probe(void) { return 0; }
namespace dummy_namespace { }
'@ | Set-Content -Encoding ASCII $tmp

    & $gcc -c $tmp -o (Join-Path $env:TEMP '_mixed_probe.o') -std=c11 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { Ok "gcc rejects C++-only syntax when compiling .c  (proves: real C mode)" }
    else                     { Bad "gcc ACCEPTED C++-only syntax in a .c file (would mean C++ mode!)" }

    # And the same file through g++ must succeed, showing the toolchains differ.
    $gpp = Join-Path $mingwBin 'g++.exe'
    & $gpp -c $tmp -o (Join-Path $env:TEMP '_mixed_probe_cpp.o') -std=c++17 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { Ok "g++ accepts the same C++-only syntax     (proves: C and C++ paths differ)" }
    else                     { Bad "g++ unexpectedly rejected valid C++ syntax" }

    Remove-Item $tmp -ErrorAction SilentlyContinue
}

Write-Host ""
if ($fail -gt 0) {
    Write-Host "RESULT: $pass passed, $fail FAILED." -ForegroundColor Red
} elseif (-not $msvcChecked) {
    # Do NOT claim a full pass when a whole section was skipped -- that is how
    # this script used to report "all checks passed" on machines where no MSVC
    # check had actually run at all.
    Write-Host "RESULT: $pass check(s) passed, but the MSVC section was SKIPPED" -ForegroundColor Yellow
    Write-Host "        (only the MinGW half was verified)." -ForegroundColor Yellow
    Write-Host "        Install VS with the C++ toolset and run tools\build_vs.bat," -ForegroundColor Yellow
    Write-Host "        then re-run this script for the full check." -ForegroundColor Yellow
    Write-Host ""
    exit 2
} else {
    Write-Host "RESULT: all $pass check(s) passed - mixed C/C++ compilation confirmed." -ForegroundColor Green
}
Write-Host ""
exit $fail

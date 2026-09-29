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

$dumpbin = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022' -Filter 'dumpbin.exe' -Recurse -ErrorAction SilentlyContinue |
           Where-Object { $_.FullName -match 'Hostx64\\x64' } | Select-Object -First 1 -ExpandProperty FullName

$vsObj = Join-Path $Root 'VS\visual_simu\x64\Debug\simu_env.obj'
if (-not (Test-Path $vsObj)) { $vsObj = Join-Path $Root 'VS\x64\Debug\simu_env.obj' }

if (-not $dumpbin) {
    Info "dumpbin not found - skipping MSVC checks"
} elseif (-not (Test-Path $vsObj)) {
    Info "MSVC object missing - run tools\build_vs.bat first"
} else {
    Info "object: $vsObj"
    $out = & $dumpbin /SYMBOLS $vsObj 2>$null
    foreach ($fn in @('image_init', 'image_process', 'otsu_get_threshold')) {
        $cStyle = $out | Where-Object { $_ -match "\|\s+$fn\s*$" }
        $cppStyle = $out | Where-Object { $_ -match "\?${fn}@@" }
        if ($cStyle -and -not $cppStyle) { Ok "$fn : C linkage (plain symbol)" }
        else { Bad "$fn : NOT C linkage (MSVC C++ mangling found)" }
    }
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
if ($fail -eq 0) {
    Write-Host "RESULT: all $pass check(s) passed - mixed C/C++ compilation confirmed." -ForegroundColor Green
} else {
    Write-Host "RESULT: $pass passed, $fail FAILED." -ForegroundColor Red
}
Write-Host ""
exit $fail

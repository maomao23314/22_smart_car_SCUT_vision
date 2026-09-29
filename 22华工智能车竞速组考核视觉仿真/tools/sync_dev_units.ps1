# =============================================================================
#  sync_dev_units.ps1 -- keep the Dev-C++ project (.dev) in sync with code\
# -----------------------------------------------------------------------------
#  New .c/.h files under code\ compile automatically (through
#  env\code_filelist.h), but they would NOT appear in the Dev-C++ project
#  tree, because the .dev file only lists the units it was created with.
#
#  This script rewrites the [UnitN] sections of  视觉仿真环境.dev  so every
#  source file shows up in the IDE's project panel. It is called from
#  gen_filelist.ps1, so you normally never run it by hand.
#
#  Rules for generated units:
#    - main.cpp / env\disp_env.cpp / env\simu_env.c : compiled (Compile=1)
#    - code\*.c                                     : Compile=0 / Link=0
#      (they are already pulled into simu_env.c via code_filelist.h;
#       compiling them again would cause duplicate-symbol errors)
#    - headers and config.h                          : Compile=0
#    - documentation (*.md) and generated files (code_filelist.h) are NOT added
#
#  File format notes (these matter):
#    - .dev must stay GBK-encoded (RedPanda reads it as ANSI); UTF-8 makes the
#      project name and tree render as mojibake
#    - .dev must use CRLF line endings
#    - UnitCount must exactly match the number of [UnitN] sections
#    - the file is only rewritten when content actually changes
#
#  NOTE: ASCII-only on purpose -- Windows PowerShell 5.1 reads .ps1 as ANSI
#        unless the file has a UTF-8 BOM.
# =============================================================================

$ErrorActionPreference = 'Stop'

$Root    = Split-Path -Parent $PSScriptRoot
$CodeDir = Join-Path $Root 'code'

# find the .dev file in the project root (name contains Chinese chars, so we
# glob instead of hard-coding the name to keep this script ASCII-only)
$devFiles = @(Get-ChildItem -Path $Root -Filter '*.dev' -File -ErrorAction SilentlyContinue)
if ($devFiles.Count -eq 0) {
    Write-Host "[sync_dev] no .dev file found, skipping" -ForegroundColor Yellow
    exit 0
}
$DevFile = $devFiles[0].FullName

$gbk = [System.Text.Encoding]::GetEncoding(936)
$text = $gbk.GetString([System.IO.File]::ReadAllBytes($DevFile))

# --- locate the [Project] header (everything before the first [UnitN) --------
$firstUnit = $text.IndexOf('[Unit1]')
if ($firstUnit -lt 0) { Write-Host "[sync_dev] no [Unit1] section found, skipping" -ForegroundColor Yellow; exit 0 }
$header = $text.Substring(0, $firstUnit)

# --- locate the [VersionInfo] footer ----------------------------------------
$vi = $text.IndexOf('[VersionInfo]')
if ($vi -lt 0) { Write-Host "[sync_dev] no [VersionInfo] section found, skipping" -ForegroundColor Yellow; exit 0 }
$footer = $text.Substring($vi)

# --- collect code\ files -----------------------------------------------------
$userC = @()
$userH = @()
if (Test-Path $CodeDir) {
    $userC = @(Get-ChildItem -Path $CodeDir -Filter '*.c' -File -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -notlike '_*' -and $_.Name -notlike '*.tmp' } | Sort-Object Name)
    $userH = @(Get-ChildItem -Path $CodeDir -Filter '*.h' -File -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -notlike '_*' -and $_.Name -notlike '*.tmp' } | Sort-Object Name)
}

# --- build the new unit list -------------------------------------------------
function UnitBlock([int]$n, [string]$path, [bool]$cpp, [bool]$compile, [bool]$link) {
    $cc = 0; if ($cpp)     { $cc = 1 }
    $co = 0; if ($compile) { $co = 1 }
    $lk = 0; if ($link)    { $lk = 1 }
    $nl = [Environment]::NewLine
    return "[Unit$n]$nl" +
           "FileName=$path$nl" +
           "CompileCpp=$cc$nl" +
           "Folder=$nl" +
           "Compile=$co$nl" +
           "Link=$lk$nl" +
           "Priority=1000$nl" +
           "OverrideBuildCmd=0$nl" +
           "BuildCmd=$nl" +
           "DetectEncoding=0$nl" +
           "Encoding=0$nl" +
           "$nl"
}

$nl  = [Environment]::NewLine
$body = New-Object System.Collections.Generic.List[string]
$n = 0

# main.cpp : the entry point
$n++; $body.Add((UnitBlock $n 'main.cpp' $true $true $true))
# config.h : user configuration (display only)
$n++; $body.Add((UnitBlock $n 'config.h' $true $false $false))
# env core
$n++; $body.Add((UnitBlock $n 'env\disp_env.hpp' $true $false $false))
$n++; $body.Add((UnitBlock $n 'env\disp_env.cpp' $true $true $true))
$n++; $body.Add((UnitBlock $n 'env\hot_reload.h' $true $false $false))
$n++; $body.Add((UnitBlock $n 'env\hot_reload.c' $false $true $true))
$n++; $body.Add((UnitBlock $n 'env\simu_env.h' $true $false $false))
$n++; $body.Add((UnitBlock $n 'env\simu_env.c' $false $true $true))
$n++; $body.Add((UnitBlock $n 'env\scut_display.h' $true $false $false))
$n++; $body.Add((UnitBlock $n 'env\scut_common_typedef.h' $true $false $false))
# user headers (display only)
foreach ($f in $userH) {
    $n++; $body.Add((UnitBlock $n ('code\' + $f.Name) $true $false $false))
}
# user sources : Compile=0 -- they are compiled via code_filelist.h
foreach ($f in $userC) {
    $n++; $body.Add((UnitBlock $n ('code\' + $f.Name) $false $false $false))
}

$newText = $header + (($body) -join '') + $footer

# --- fix UnitCount in the header ---------------------------------------------
#     $header came from the old file, so its UnitCount is stale.
$total = $n
$newText = [regex]::Replace($newText, 'UnitCount=\d+', "UnitCount=$total")

# --- normalize line endings to CRLF and compare ------------------------------
$newText = ($newText -replace "`r`n", "`n") -replace "`n", "`r`n"

if ($text -ne $newText) {
    [System.IO.File]::WriteAllBytes($DevFile, $gbk.GetBytes($newText))
    Write-Host ("[sync_dev] .dev units updated ({0} unit(s))" -f $n) -ForegroundColor Cyan
} else {
    Write-Host ("[sync_dev] .dev up to date ({0} unit(s))" -f $n) -ForegroundColor Cyan
}

exit 0

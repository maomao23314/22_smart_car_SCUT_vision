# =============================================================================
#  sync_vs_items.ps1 -- keep the VS project in sync with the files on disk
# -----------------------------------------------------------------------------
#  The VS project deliberately avoids wildcards like ..\code\*.c, because VS
#  warns "VC projects do not support wildcards in project item definitions"
#  and such wildcards can make the IDE behave unexpectedly.
#
#  Instead, this script rewrites the <ClCompile>/<ClInclude> item groups of
#      VS\visual_simu.vcxproj
#      VS\visual_simu.vcxproj.filters
#  so every file under code\ shows up in Solution Explorer, while the actual
#  compilation still happens through env\code_filelist.h (so a new .c file is
#  built even if you never run this script).
#
#  Called automatically by gen_filelist.ps1, so you normally don't run it
#  by hand.
#
#  NOTE: this file is ASCII-only on purpose -- Windows PowerShell 5.1 reads
#        .ps1 files as ANSI unless they carry a UTF-8 BOM.
# =============================================================================

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
$VsDir = Join-Path $Root 'VS'
$Proj  = Join-Path $VsDir 'visual_simu.vcxproj'
$Filt  = Join-Path $VsDir 'visual_simu.vcxproj.filters'
$Code  = Join-Path $Root 'code'

if (-not (Test-Path $Proj)) { Write-Host "[sync_vs] no VS project, skipping" -ForegroundColor Yellow; exit 0 }
if (-not (Test-Path $Filt)) { Write-Host "[sync_vs] no filters file, skipping" -ForegroundColor Yellow; exit 0 }

# --- collect user sources under code\ ---------------------------------------
# NOTE: -Recurse and the '*.tmp' filter MUST match gen_filelist.ps1, otherwise
# the files that get COMPILED and the files shown in Solution Explorer differ.
# Paths are kept RELATIVE to code\ (forward slashes -> backslashes for MSBuild)
# so that sub-directories work: code\algo\line.c -> ..\code\algo\line.c
$userC = @()
$userH = @()
if (Test-Path $Code) {
    $userC = @(Get-ChildItem -Path $Code -Filter '*.c' -Recurse -File -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -notlike '_*' -and $_.Name -notlike '*.tmp' } | Sort-Object FullName)
    $userH = @(Get-ChildItem -Path $Code -Filter '*.h' -Recurse -File -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -notlike '_*' -and $_.Name -notlike '*.tmp' } | Sort-Object FullName)
}

# relative path under code\, with backslashes (MSBuild item paths)
function Get-Rel([string]$full, [string]$baseDir) {
    return ($full.Substring($baseDir.Length).TrimStart('\', '/') -replace '/', '\')
}

$nl = "`r`n"

# --- rebuild the ClCompile / ClInclude item groups in the .vcxproj -----------
$projText = [System.IO.File]::ReadAllText($Proj)

$compileItems = New-Object System.Collections.Generic.List[string]
$compileItems.Add('    <ClCompile Include="..\main.cpp" />')
$compileItems.Add('    <ClCompile Include="..\env\disp_env.cpp" />')
$compileItems.Add('    <ClCompile Include="..\env\simu_env.c">')
$compileItems.Add('      <CompileAs>CompileAsC</CompileAs>')
$compileItems.Add('    </ClCompile>')
# NOTE: env\hot_reload.c is NOT listed here on purpose. It is a fixed env file
#       and lives in a static ItemGroup ABOVE this auto-generated block, because
#       items added by this script during a build are too late for MSBuild to
#       compile them (would break the first build after a fresh checkout).
foreach ($f in $userC) {
    $compileItems.Add('    <ClCompile Include="..\code\' + (Get-Rel $f.FullName $Code) + '">')
    $compileItems.Add('      <CompileAs>CompileAsC</CompileAs>')
    # already pulled in via code_filelist.h -> do not compile twice
    $compileItems.Add('      <ExcludedFromBuild>true</ExcludedFromBuild>')
    $compileItems.Add('    </ClCompile>')
}

$includeItems = New-Object System.Collections.Generic.List[string]
$includeItems.Add('    <ClInclude Include="..\config.h" />')
$includeItems.Add('    <ClInclude Include="..\env\disp_env.hpp" />')
# env\hot_reload.h also lives in the static ItemGroup above (see NOTE).
$includeItems.Add('    <ClInclude Include="..\env\simu_env.h" />')
$includeItems.Add('    <ClInclude Include="..\env\scut_common_typedef.h" />')
$includeItems.Add('    <ClInclude Include="..\env\scut_display.h" />')
$includeItems.Add('    <ClInclude Include="..\env\code_filelist.h" />')
foreach ($f in $userH) {
    $includeItems.Add('    <ClInclude Include="..\code\' + (Get-Rel $f.FullName $Code) + '" />')
}

$newGroups = '  <!-- ==== BEGIN AUTO-GENERATED ITEMS (tools\sync_vs_items.ps1) ==== -->' + $nl +
             '  <ItemGroup>' + $nl + ($compileItems -join $nl) + $nl + '  </ItemGroup>' + $nl +
             '  <ItemGroup>' + $nl + ($includeItems -join $nl) + $nl + '  </ItemGroup>' + $nl +
             '  <!-- ==== END AUTO-GENERATED ITEMS ==== -->'

$begin = '  <!-- ==== BEGIN AUTO-GENERATED ITEMS'
$end   = '  <!-- ==== END AUTO-GENERATED ITEMS ==== -->'
$i = $projText.IndexOf($begin)
$j = $projText.IndexOf($end)
if ($i -ge 0 -and $j -gt $i) {
    $projNew = $projText.Substring(0, $i) + $newGroups + $projText.Substring($j + $end.Length)

    # Only write when the content actually changed.
    # Writing unconditionally would bump the file's timestamp on every build,
    # and since visual_simu.vcxproj is normally open in the VS editor, VS would
    # then pop up "This file has been modified outside the source editor -
    # reload?" every time you build or start/stop debugging.
    if ($projText -ne $projNew) {
        [System.IO.File]::WriteAllText($Proj, $projNew, (New-Object System.Text.UTF8Encoding($true)))
        Write-Host ("[sync_vs] vcxproj item groups updated ({0} .c, {1} .h)" -f $userC.Count, $userH.Count) -ForegroundColor Cyan
    } else {
        Write-Host ("[sync_vs] vcxproj up to date ({0} .c, {1} .h)" -f $userC.Count, $userH.Count) -ForegroundColor Cyan
    }
} else {
    Write-Host "[sync_vs] marker not found in vcxproj - skipped" -ForegroundColor Yellow
}

# --- rebuild the .filters file ----------------------------------------------
$filtLines = New-Object System.Collections.Generic.List[string]
$filtLines.Add('<?xml version="1.0" encoding="utf-8"?>')
$filtLines.Add('<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
$filtLines.Add('  <ItemGroup>')
$filtLines.Add('    <Filter Include="env">')
$filtLines.Add('      <UniqueIdentifier>{11111111-1111-1111-1111-111111111111}</UniqueIdentifier>')
$filtLines.Add('    </Filter>')
$filtLines.Add('    <Filter Include="code">')
$filtLines.Add('      <UniqueIdentifier>{22222222-2222-2222-2222-222222222222}</UniqueIdentifier>')
$filtLines.Add('    </Filter>')
$filtLines.Add('    <Filter Include="config">')
$filtLines.Add('      <UniqueIdentifier>{33333333-3333-3333-3333-333333333333}</UniqueIdentifier>')
$filtLines.Add('    </Filter>')
$filtLines.Add('  </ItemGroup>')
$filtLines.Add('  <ItemGroup>')
$filtLines.Add('    <ClCompile Include="..\main.cpp" />')
$filtLines.Add('  </ItemGroup>')
$filtLines.Add('  <ItemGroup>')
$filtLines.Add('    <ClCompile Include="..\env\disp_env.cpp">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClCompile>')
$filtLines.Add('    <ClCompile Include="..\env\simu_env.c">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClCompile>')
# hot_reload.c / .h appear here ONLY (filters are display-only, so listing them
# here does not cause the double-compile problem that the .vcxproj ItemGroup would).
$filtLines.Add('    <ClCompile Include="..\env\hot_reload.c">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClCompile>')
foreach ($f in $userC) {
    $filtLines.Add('    <ClCompile Include="..\code\' + (Get-Rel $f.FullName $Code) + '">')
    $filtLines.Add('      <Filter>code</Filter>')
    $filtLines.Add('    </ClCompile>')
}
$filtLines.Add('  </ItemGroup>')
$filtLines.Add('  <ItemGroup>')
$filtLines.Add('    <ClInclude Include="..\config.h">')
$filtLines.Add('      <Filter>config</Filter>')
$filtLines.Add('    </ClInclude>')
$filtLines.Add('    <ClInclude Include="..\env\disp_env.hpp">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClInclude>')
$filtLines.Add('    <ClInclude Include="..\env\hot_reload.h">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClInclude>')
$filtLines.Add('    <ClInclude Include="..\env\simu_env.h">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClInclude>')
$filtLines.Add('    <ClInclude Include="..\env\scut_common_typedef.h">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClInclude>')
$filtLines.Add('    <ClInclude Include="..\env\scut_display.h">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClInclude>')
$filtLines.Add('    <ClInclude Include="..\env\code_filelist.h">')
$filtLines.Add('      <Filter>env</Filter>')
$filtLines.Add('    </ClInclude>')
foreach ($f in $userH) {
    $filtLines.Add('    <ClInclude Include="..\code\' + (Get-Rel $f.FullName $Code) + '">')
    $filtLines.Add('      <Filter>code</Filter>')
    $filtLines.Add('    </ClInclude>')
}
$filtLines.Add('  </ItemGroup>')
$filtLines.Add('</Project>')

$filtNew = ($filtLines -join $nl) + $nl
$filtOld = ''
if (Test-Path $Filt) { $filtOld = [System.IO.File]::ReadAllText($Filt) }

if ($filtOld -ne $filtNew) {
    [System.IO.File]::WriteAllText($Filt, $filtNew, (New-Object System.Text.UTF8Encoding($true)))
    Write-Host "[sync_vs] filters updated" -ForegroundColor Cyan
}

exit 0

@echo off
rem ============================================================================
rem  find_msbuild.bat -- locate MSBuild for ANY Visual Studio version
rem ----------------------------------------------------------------------------
rem  Usage:  call "%~dp0find_msbuild.bat"
rem          if not defined MSB ( echo no VS found )
rem
rem  Search order:
rem    1. vswhere.exe  -- the official VS locator. It ships with VS 2017+ and
rem       is version-agnostic, so it finds 2017 / 2019 / 2022 / 2026 / whatever
rem       is installed, in any edition (Community / Pro / Enterprise / BuildTools)
rem       and any install location (including D:\).
rem    2. Hard-coded fallbacks for the common layouts, newest first. These only
rem       matter when vswhere is missing (very old or hand-copied installs).
rem
rem  Why not hard-code only: the previous version listed just the four VS2022
rem  default paths, so it reported "no Visual Studio" on any other version,
rem  custom install drive, or Build Tools setup.
rem
rem  NOTE: ASCII-only on purpose -- .bat files are read as ANSI (codepage 936)
rem        on Chinese Windows, so non-ASCII here would garble the parser.
rem
rem  EasyX preference: when several VS installs exist (common: both Build Tools
rem  and Community), pick one that actually has easyx.h in its VC include dir.
rem  The EasyX installer only targets full IDE installs, so a plain Build Tools
rem  setup usually lacks it -- choosing that first would make the build fail
rem  with "cannot open include file: easyx.h".
rem ============================================================================
setlocal enabledelayedexpansion
set "MSB="
set "MSB_FALLBACK="
set "VSREL="

rem ---- 1. vswhere (preferred) ------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    rem  -products * : include Build Tools, not just full IDE
    rem  -prerelease : also match preview/insider builds (e.g. a future "2026")
    rem  -sort        : newest first, so we prefer the latest toolset
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -prerelease -products * -requires Microsoft.Component.MSBuild -sort -property installationPath 2^>nul`) do (
        set "CAND="
        if exist "%%I\MSBuild\Current\Bin\MSBuild.exe" set "CAND=%%I\MSBuild\Current\Bin\MSBuild.exe"
        if not defined CAND if exist "%%I\MSBuild\17.0\Bin\MSBuild.exe" set "CAND=%%I\MSBuild\17.0\Bin\MSBuild.exe"
        if not defined CAND if exist "%%I\MSBuild\16.0\Bin\MSBuild.exe" set "CAND=%%I\MSBuild\16.0\Bin\MSBuild.exe"
        if not defined CAND if exist "%%I\MSBuild\15.0\Bin\MSBuild.exe" set "CAND=%%I\MSBuild\15.0\Bin\MSBuild.exe"

        if defined CAND (
            rem  Remember the first (newest) one as a fallback whatever happens.
            if not defined MSB_FALLBACK set "MSB_FALLBACK=!CAND!"
            rem  Prefer one that has EasyX available for the compiler.
            if not defined MSB (
                if exist "%%I\VC\Auxiliary\VS\include\easyx.h" set "MSB=!CAND!"
                for /d %%V in ("%%I\VC\Tools\MSVC\*") do (
                    if not defined MSB if exist "%%~fV\include\easyx.h" set "MSB=!CAND!"
                )
            )
        )
    )
)

rem  No EasyX-equipped install found -> fall back to the newest one anyway.
rem  (The user may have EasyX somewhere else, or only want to compile-check.)
if not defined MSB if defined MSB_FALLBACK set "MSB=!MSB_FALLBACK!"

rem ---- 2. hard-coded fallbacks, newest first ---------------------------------
if not defined MSB (
    for %%P in (
        "C:\Program Files\Microsoft Visual Studio\2026\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2026\Professional\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe"
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe"
    ) do (
        if not defined MSB if exist %%P set "MSB=%%~P"
    )
)

rem ---- 3. last resort: whatever msbuild.exe is on PATH -----------------------
if not defined MSB (
    for /f "delims=" %%I in ('where msbuild.exe 2^>nul') do (
        if not defined MSB set "MSB=%%I"
    )
)

if defined MSB (
    rem  Report which VS release it is, for the user's peace of mind.
    if exist "%VSWHERE%" (
        for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -prerelease -products * -property catalog_productDisplayVersion 2^>nul`) do set "VSREL=%%I"
    )
    if defined VSREL (echo [find_msbuild] Visual Studio !VSREL!) else (echo [find_msbuild] MSBuild found)
    echo [find_msbuild] ^-^> !MSB!
    rem  Say whether EasyX is ready, so a later failure is easier to diagnose.
    if exist "!MSB!\..\..\..\..\VC\Auxiliary\VS\include\easyx.h" (
        echo [find_msbuild] EasyX: found
    ) else (
        set "EXFOUND="
        for /d %%V in ("!MSB!\..\..\..\..\VC\Tools\MSVC\*") do (
            if exist "%%~fV\include\easyx.h" set "EXFOUND=1"
        )
        if defined EXFOUND (echo [find_msbuild] EasyX: found) else (echo [find_msbuild] EasyX: NOT found for this VS - install it, or use build_dev.bat)
    )
)

rem  Propagate MSB past endlocal. %MSB% is expanded by the parser BEFORE
rem  endlocal runs (single-line compound command), so the caller receives the
rem  value found above -- not the empty pre-setlocal one.
endlocal & set "MSB=%MSB%"
exit /b 0

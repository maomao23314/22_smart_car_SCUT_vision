@echo off
rem ============================================================================
rem  一键配置.bat  --  run this ONCE after copying the project to a new machine
rem ----------------------------------------------------------------------------
rem  It does four things:
rem    1. scans code\ and regenerates env\code_filelist.h
rem    2. finds a usable compiler
rem         - Dev-C++ / MinGW-w64   (any install path, or MINGW_HOME)
rem         - Visual Studio         (ANY version and edition, via vswhere)
rem    3. checks that EasyX is installed for the compiler(s) it found
rem    4. optionally builds and runs right away
rem
rem  You only need ONE of the two toolchains. If both are present you can pick
rem  which one to build with in step 4.
rem
rem  NOTE: ASCII-only on purpose. .bat files are read as ANSI (codepage 936) on
rem        Chinese Windows, so Chinese text here would garble the parser.
rem        The Chinese filename is fine -- only the CONTENT must stay ASCII.
rem ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================================
echo    22nd Visual Simulation Environment  -  First-time Setup
echo ============================================================
echo.

rem ---------------------------------------------------------------------------
rem  [1/4] regenerate env\code_filelist.h
rem ---------------------------------------------------------------------------
echo [1/4] Scanning code\ and regenerating env\code_filelist.h ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\gen_filelist.ps1"
if errorlevel 1 (
    echo       [FAILED] could not regenerate code_filelist.h
    goto :end
)
echo.

rem ---------------------------------------------------------------------------
rem  [2/4] locate compilers
rem ---------------------------------------------------------------------------
echo [2/4] Looking for a compiler ...

rem ---- Dev-C++ / MinGW-w64 ---------------------------------------------------
rem  Checked in order: MINGW_HOME, then the usual install locations. The
rem  directory may be either "<root>\bin" (has g++.exe directly) or "<root>"
rem  (Dev-C++ layout: <root>\MinGW64\bin). Both are handled.
set "MINGW="
if defined MINGW_HOME (
    if exist "%MINGW_HOME%\bin\g++.exe" set "MINGW=%MINGW_HOME%\bin"
    if not defined MINGW if exist "%MINGW_HOME%\MinGW64\bin\g++.exe" set "MINGW=%MINGW_HOME%\MinGW64\bin"
)
for %%R in (
    "D:\TOOLS\Dev-Cpp"
    "C:\Program Files (x86)\Dev-Cpp"
    "C:\Program Files\Dev-Cpp"
    "D:\Dev-Cpp"
    "D:\TOOLS\mingw-w64"
    "C:\mingw64"
    "C:\msys64\mingw64"
) do (
    if not defined MINGW if exist "%%~R\bin\g++.exe"      set "MINGW=%%~R\bin"
    if not defined MINGW if exist "%%~R\MinGW64\bin\g++.exe" set "MINGW=%%~R\MinGW64\bin"
)
if not defined MINGW (
    for /f "delims=" %%I in ('where g++.exe 2^>nul') do (
        if not defined MINGW set "MINGW=%%~dpI"
    )
)
rem strip a trailing backslash so "%MINGW%\gcc.exe" stays well-formed
if defined MINGW if "%MINGW:~-1%"=="\" set "MINGW=%MINGW:~0,-1%"

rem ---- Visual Studio (any version / edition) ---------------------------------
rem  Delegated to tools\find_msbuild.bat so build_vs.bat and this script can
rem  never disagree about which VS is installed. It echoes what it found and
rem  exports MSB via "endlocal & set".
set "MSB="
call "%~dp0tools\find_msbuild.bat"

if defined MINGW (
    echo       [OK] Dev-C++ / MinGW-w64 : !MINGW!
    for /f "tokens=*" %%V in ('"!MINGW!\g++.exe" -dumpversion 2^>nul') do echo            g++ %%V
) else (
    echo       [--] Dev-C++ / MinGW-w64 not found
)

if defined MSB (
    echo       [OK] Visual Studio  ^(MSBuild^)
    echo            !MSB!
) else (
    echo       [--] Visual Studio not found
)

if not defined MINGW if not defined MSB (
    echo.
    echo       [ERROR] No compiler found.
    echo               Install ONE of:
    echo                 * Dev-C++ 6.7.5 ^(with MinGW-w64^)  - see dev config guide
    echo                 * Visual Studio with "Desktop development with C++"
    echo               Then run this script again.
    goto :end
)
echo.

rem ---------------------------------------------------------------------------
rem  [3/4] check EasyX
rem ---------------------------------------------------------------------------
echo [3/4] Checking EasyX ...
set "HASEX=0"
if defined MINGW (
    rem  easyx.h may sit in <mingw>\include or <mingw>\x86_64-w64-mingw32\include
    if exist "!MINGW!\..\include\easyx.h"                            set "HASEX=1"
    if exist "!MINGW!\..\x86_64-w64-mingw32\include\easyx.h"         set "HASEX=1"
    if exist "!MINGW!\include\easyx.h"                               set "HASEX=1"
    if "!HASEX!"=="1" (
        echo       [OK] EasyX for MinGW
    ) else (
        echo       [!!] easyx.h NOT found for MinGW
        echo            Copy include\easyx.h + lib64\libeasyx.a from
        echo            easyx4mingw_*.zip - see dev config guide.
    )
)

set "HASEXVS=0"
if defined MSB (
    rem  Look for EasyX in the VC include dirs of the detected VS install.
    for %%D in ("!MSB!\..\..\..\..\VC\Auxiliary\VS\include") do (
        if exist "%%~fD\easyx.h" set "HASEXVS=1"
    )
    if "!HASEXVS!"=="1" (
        echo       [OK] EasyX for Visual Studio
    ) else (
        echo       [!!] easyx.h NOT found for Visual Studio
        echo            Install EasyX from the official site ^(VS build^),
        echo            or use the Dev-C++ / MinGW toolchain instead.
    )
)
echo.

rem ---------------------------------------------------------------------------
rem  [4/4] optional immediate build
rem ---------------------------------------------------------------------------
echo [4/4] Build now?
if defined MINGW echo       1 = build and run with Dev-C++ / MinGW
if defined MSB   echo       2 = build and run with Visual Studio
echo       0 = skip
set "CHOICE="
set /p "CHOICE=Enter 0/1/2 then press Enter [default 0]: "
if "%CHOICE%"=="1" if defined MINGW call "%~dp0tools\build_dev.bat"
if "%CHOICE%"=="2" if defined MSB   call "%~dp0tools\build_vs.bat" Release
echo.

echo ============================================================
echo   Setup complete. From now on:
echo     * command line : tools\build_dev.bat   or   tools\build_vs.bat
echo     * Dev-C++      : open the .dev file, press F11
echo     * Visual Studio: open VS\visual_simu.sln, press F5
echo.
echo   Your code goes in code\ - new .c/.h files are picked up
echo   automatically, no need to edit any project file.
echo.
echo   IMPORTANT (Dev-C++ users): Dev-C++ has no pre-build step, so
echo   pressing F11 does NOT rescan code\. After adding/removing a
echo   .c or .h under code\, run once:
echo       tools\rebuild.bat
echo   then press F11 as usual. (Visual Studio does this by itself
echo   on every F5, and this setup script just did it for you too.)
echo.
echo   Hot reload: save a new version of the image the environment is
echo   currently showing (e.g. from the image editor) and it reloads
echo   and re-runs by itself. Press H in the window to toggle it.
echo ============================================================

:end
echo.
pause
endlocal
exit /b 0

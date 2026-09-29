echo off
rem ============================================================================
rem  build_dev.bat -- one-click build with the Dev-C++ / MinGW-w64 toolchain
rem ----------------------------------------------------------------------------
rem  Builds a REAL mixed C/C++ program:
rem      code\*.c        compiled as C   (gcc -std=c11)
rem      env\simu_env.c  compiled as C   (gcc)   <- pulls in code_filelist.h
rem      main.cpp        compiled as C++ (g++)
rem      env\*.cpp       compiled as C++ (g++)
rem
rem  Output: build\visual_simu.exe
rem  Usage : tools\build_dev.bat [norun]
rem ============================================================================
setlocal enabledelayedexpansion
chcp 936 >nul 2>&1

cd /d "%~dp0.."
set "ROOT=%CD%"

rem ---------------------------- 1. locate the compiler -----------------------
set "MINGW="
if defined MINGW_HOME if exist "%MINGW_HOME%\bin\g++.exe" set "MINGW=%MINGW_HOME%\bin"
if not defined MINGW if exist "D:\TOOLS\Dev-Cpp\MinGW64\bin\g++.exe" set "MINGW=D:\TOOLS\Dev-Cpp\MinGW64\bin"
if not defined MINGW if exist "C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\g++.exe" set "MINGW=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin"
if not defined MINGW if exist "C:\Program Files\Dev-Cpp\MinGW64\bin\g++.exe" set "MINGW=C:\Program Files\Dev-Cpp\MinGW64\bin"
if not defined MINGW if exist "D:\TOOLS\mingw-w64\bin\g++.exe" set "MINGW=D:\TOOLS\mingw-w64\bin"

if not defined MINGW (
    echo [ERROR] Cannot find the Dev-C++ / MinGW-w64 compiler.
    echo         Set MINGW_HOME to the folder containing g++.exe, then retry.
    echo         Example:  set MINGW_HOME=D:\TOOLS\Dev-Cpp\MinGW64
    exit /b 1
)

set "GCC=%MINGW%\gcc.exe"
set "GPP=%MINGW%\g++.exe"
echo [build_dev] compiler : %MINGW%

rem ---------------------------- 2. regenerate the code file list --------------
echo [build_dev] scanning code\ ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\gen_filelist.ps1"
if errorlevel 1 ( echo [ERROR] gen_filelist.ps1 failed & exit /b 1 )

rem ---------------------------- 3. prepare build dir --------------------------
if not exist "%ROOT%\build" mkdir "%ROOT%\build"
set "OUT=%ROOT%\build"

rem Source files are UTF-8 with BOM; console text is GBK on Chinese Windows
set "ENC=-finput-charset=UTF-8 -fexec-charset=GBK"
set "WARN=-Wall -Wno-unused-parameter"
rem NOTE: -I paths must not be wrapped in quotes here; the project path may contain
rem       CJK characters but no spaces, and quoting breaks the -I flag in cmd.
set INC=-I%ROOT%\env -I%ROOT% -I%ROOT%\code

rem ---------------------------- 4. compile C sources ---------------------------
rem  code\*.c are NOT compiled individually here: they are pulled into
rem  simu_env.c through env\code_filelist.h (generated in step 2).
rem  That is exactly the "new files get included automatically" mechanism,
rem  so compiling them separately would cause duplicate-symbol link errors.
rem
rem  env\*.c (simu_env.c, hot_reload.c, ...) ARE compiled individually.
rem  The list is DISCOVERED, not hard-coded -- hard-coding it is what made an
rem  earlier build silently miss env\hot_reload.c and then fail to link with
rem  "undefined reference to SCUT_HotReloadInit".
echo [build_dev] compiling C sources with gcc ...
set "COBJS="
for %%F in ("%ROOT%\env\*.c") do (
    echo    [C ] %%~nxF
    "%GCC%" -c "%%~fF" -o "%OUT%\%%~nF.o" -std=c11 %ENC% %WARN% %INC%
    if errorlevel 1 goto :fail
    set "COBJS=!COBJS! "%OUT%\%%~nF.o""
)

rem ---------------------------- 5. compile C++ sources -------------------------
rem  Also discovered rather than hard-coded, for the same reason.
echo [build_dev] compiling C++ sources with g++ ...
set "CXXOBJS="
for %%F in ("%ROOT%\*.cpp" "%ROOT%\env\*.cpp") do (
    echo    [C++] %%~nxF
    "%GPP%" -c "%%~fF" -o "%OUT%\%%~nF.o" -std=c++17 %ENC% %WARN% %INC%
    if errorlevel 1 goto :fail
    set "CXXOBJS=!CXXOBJS! "%OUT%\%%~nF.o""
)

rem ---------------------------- 6. link ---------------------------------------
rem  Do NOT pass -mwindows: USE_CONSOLE in config.h decides whether the console
rem  shows up (Environment EX_SHOWCONSOLE), and printf() diagnostics need a
rem  console subsystem to be visible. A GUI-subsystem binary would drop them.
echo [build_dev] linking ...
"%GPP%" %COBJS% %CXXOBJS% -o "%OUT%\visual_simu.exe" -leasyx -lgdi32 -lole32 -static
if errorlevel 1 goto :fail

rem ---------------------------- 7. done --------------------------------------
rem  No need to copy pic\ next to the exe: the program chdir's to the project
rem  root at startup and reads images by relative path ("pic/1/1.bmp").
echo.
echo [build_dev] OK  -^>  %OUT%\visual_simu.exe
echo.

rem ---------------------------- 8. run ----------------------------------------
if /i "%~1"=="norun" goto :done
echo [build_dev] launching ...
pushd "%OUT%"
start "" "%OUT%\visual_simu.exe"
popd
goto :done

:fail
echo.
echo [ERROR] build failed
endlocal
exit /b 1

:done
endlocal
exit /b 0

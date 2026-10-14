@echo off
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
rem NOTE: relative -I paths are used on purpose, and they are correct here
rem       because line 17 above did "cd /d %~dp0.." -- the working directory is
rem       the project root by the time the compiler runs.
rem       Relative paths (a) match what the .dev project and the config guide
rem       already use, and (b) keep working when the project sits in a path
rem       containing spaces (e.g. OneDrive folders): an ABSOLUTE -I path would
rem       be split at the space unless quoted, producing bogus errors like
rem         gcc: error: Files/...: No such file or directory
rem       Do NOT "fix" this by wrapping %ROOT% in quotes -- just keep it relative.
set INC=-Ienv -Icode -I.

rem ---------------------------- 3b. optimisation --------------------------------
rem  ? ?????code\*.c???? -O0 ???????????
rem
rem  ????
rem    1) ???????????????????????????
rem       GCC ????????????????????????????
rem       ???????????????????????????
rem       ???????CH32 ?????????????????
rem       ???? -O0 ???????????
rem    2) ???????????????? -O0???????"???"?
rem
rem  ???code\*.c ??? env\code_filelist.h ? simu_env.c ??????
rem        ?????????????????????? code\ ??????
rem        ?? env\ ?? C ???simu_env.c / hot_reload.c?????????
rem        ??? -O0 ???????? Dev-C++ / VS Debug ???
rem
rem  ????????????????Release ??????? -O0 ??????
set "COPT=-O0"

rem ---------------------------- 4. compile C sources ---------------------------
rem  code\*.c are NOT compiled individually here: they are pulled into
rem  simu_env.c through env\code_filelist.h (generated in step 2).
rem  That is exactly the "new files get included automatically" mechanism,
rem  so compiling them separately would cause duplicate-symbol link errors.
rem
rem  env\*.c (simu_env.c, hot_reload.c, ...) ARE compiled individually.
rem  The list is DISCOVERED, not hard-coded -- hard-coding it is what made an
rem  earlier build silently miss env\hot_reload.c and then fail to link with
rem  "undefined reference to LM_HotReloadInit".
echo [build_dev] compiling C sources with gcc ...
set "COBJS="
for %%F in ("%ROOT%\env\*.c") do (
    echo    [C ] %%~nxF
    "%GCC%" -c "%%~fF" -o "%OUT%\%%~nF.o" -std=c11 %COPT% %ENC% %WARN% %INC%
    if errorlevel 1 goto :fail
    set "COBJS=!COBJS! "%OUT%\%%~nF.o""
)

rem ---------------------------- 5. compile C++ sources -------------------------
rem  Also discovered rather than hard-coded, for the same reason.
rem  env\ ? main.cpp ???????????????????????
rem  ?? -O0 ??????????????????
echo [build_dev] compiling C++ sources with g++ ...
set "CXXOBJS="
for %%F in ("%ROOT%\*.cpp" "%ROOT%\env\*.cpp") do (
    echo    [C++] %%~nxF
    "%GPP%" -c "%%~fF" -o "%OUT%\%%~nF.o" -std=c++17 %COPT% %ENC% %WARN% %INC%
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
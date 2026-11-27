@echo off
setlocal
rem Development build for Windows. Extra arguments are passed to CMake, e.g.
rem   build_develop.bat -DSS_BACKEND=vulkan -DSS_BUILD_GUI=OFF
rem Warnings fail the compile. --allow-warnings only prints them, a way past
rem a warning your compiler raises and ours do not. --warnings-at-end (CI)
rem builds to the end and then fails, so the log lists every warning.
rem Builds the standalone spirula.exe into build_cuda\ or build_vulkan\.
rem Works from a plain cmd prompt: locates VS via vswhere and calls vcvars64,
rem uses the VS-bundled CMake/Ninja when none are on PATH, and picks the
rem newest installed CUDA toolkit unless CUDA_PATH is already set.

cd /d "%~dp0"

rem ---------------------------------------------------------------------------
rem One tree per backend, so building one does not reconfigure the other. The
rem arguments are read, never rebuilt: your own -B still reaches CMake, and the
rem build step below would then target a tree it did not configure.
rem ---------------------------------------------------------------------------
set "BACKENDSEL=cuda"
echo %* | findstr /i /c:"BACKEND=vulkan" >nul && set "BACKENDSEL=vulkan"
set "BUILDDIR=build_%BACKENDSEL%"

rem The warning flags are cut out of the text: cmd would split -DX=Y at its "=".
set "SSARGS=%*"
set "WARNMODE=error"
echo %* | findstr /i /c:"--warnings-at-end" >nul || goto :no_at_end
set "WARNMODE=at-end"
set "SSARGS=%SSARGS:--warnings-at-end=%"
:no_at_end
echo %* | findstr /i /c:"--allow-warnings" >nul || goto :no_allow
set "WARNMODE=allow"
set "SSARGS=%SSARGS:--allow-warnings=%"
:no_allow
set "WERROR=ON"
if not "%WARNMODE%"=="error" set "WERROR=OFF"
if not "%WARNMODE%"=="at-end" goto :args_done
where python >nul 2>&1 && goto :args_done
echo --warnings-at-end needs python
exit /b 1
:args_done

rem ---------------------------------------------------------------------------
rem Codegen (optional -- the generated files are committed)
rem ---------------------------------------------------------------------------
where python >nul 2>&1 || goto :skip_codegen
python tools\codegen\generate_headers.py || goto :codegen_warn
python tools\codegen\generate_kernel_instantiation.py || goto :codegen_warn
goto :comment_check
:codegen_warn
echo codegen failed -- continuing with committed generated files
goto :comment_check
:skip_codegen
echo python not found -- skipping codegen (using committed generated files)

rem ---------------------------------------------------------------------------
rem Comment-length gate (AGENTS.md), and no source naming a windows.h macro.
rem Both are also wired into CMake (cmake/SsChecks.cmake), which covers a bare
rem cmake/ninja build; running them here fails before the configure step.
rem ---------------------------------------------------------------------------
:comment_check
where python >nul 2>&1 || goto :msvc_env
python tools\check_comment_length.py || exit /b 1
python tools\check_winmacro.py --quiet || exit /b 1

rem ---------------------------------------------------------------------------
rem MSVC environment. vcvars64 is called even when cl is already on PATH: a
rem machine-wide cl/INCLUDE (e.g. exported from an old developer prompt) can
rem point at an uninstalled Windows SDK, which breaks compiles with missing
rem stddef.h/stdlib.h. Re-initializing is harmless in a developer prompt.
rem ---------------------------------------------------------------------------
:msvc_env
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT set "VSROOT=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto :have_cl
)
where cl >nul 2>&1 && goto :have_cl
echo ERROR: vcvars64.bat not found -- install Visual Studio with C++ tools,
echo        or run this script from a developer command prompt.
exit /b 1
:have_cl

rem ---------------------------------------------------------------------------
rem CMake + Ninja (fall back to the copies bundled with Visual Studio)
rem ---------------------------------------------------------------------------
where cmake >nul 2>&1
if errorlevel 1 set "PATH=%PATH%;%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
where ninja >nul 2>&1
if errorlevel 1 set "PATH=%PATH%;%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

rem ---------------------------------------------------------------------------
rem CUDA toolkit: newest installed version. An ambient CUDA_PATH is NOT
rem trusted by default (it may pin a toolkit older than the MSVC in use);
rem pass a trailing -DCMAKE_CUDA_COMPILER=... to pick a specific one.
rem A Vulkan build never enables the CUDA language, so passing the compiler
rem there only earns a "manually-specified variables were not used" warning.
rem ---------------------------------------------------------------------------
if /i "%BACKENDSEL%"=="vulkan" goto :no_cuda
set "_CUDA_ROOT="
for /d %%d in ("%ProgramFiles%\NVIDIA GPU Computing Toolkit\CUDA\v*") do set "_CUDA_ROOT=%%d"
if not defined _CUDA_ROOT set "_CUDA_ROOT=%CUDA_PATH%"
set "CUDAARG="
if defined _CUDA_ROOT (
    set "CUDA_PATH=%_CUDA_ROOT%"
    set "PATH=%_CUDA_ROOT%\bin;%PATH%"
    set CUDAARG=-DCMAKE_CUDA_COMPILER="%_CUDA_ROOT%\bin\nvcc.exe"
)
goto :have_cuda
:no_cuda
set "CUDAARG="
:have_cuda

rem ---------------------------------------------------------------------------
rem Configure + build (RAM-aware job count, mirrors build_develop.bash)
rem ---------------------------------------------------------------------------
rem After the default, so an explicit -DSS_WARNINGS_AS_ERRORS=OFF wins.
cmake -G Ninja -B "%BUILDDIR%" -DCMAKE_BUILD_TYPE=Release -DSS_WARNINGS_AS_ERRORS=%WERROR% %CUDAARG% %SSARGS%
if %ERRORLEVEL% neq 0 exit /b 1

rem ---------------------------------------------------------------------------
rem Repair the ninja dependency log. build_develop.bash carries the long form:
rem a record with a mismatched node index survives ninja's own recovery, and
rem from then on every build discards its header dependencies and the next one
rem recompiles every object. `-t recompact` rewrites the log and drops it.
rem ---------------------------------------------------------------------------
if not exist "%BUILDDIR%\.ninja_deps" goto :deps_done
cmake --build "%BUILDDIR%" -- -t recompact >"%TEMP%\ss_ninja_deps.log" 2>&1
findstr /c:"premature end of file" /c:"bad deps log" "%TEMP%\ss_ninja_deps.log" >nul
if errorlevel 1 goto :deps_checked
rem The rewrite is what heals it, so a second pass is what confirms it.
cmake --build "%BUILDDIR%" -- -t recompact >"%TEMP%\ss_ninja_deps.log" 2>&1
findstr /c:"premature end of file" /c:"bad deps log" "%TEMP%\ss_ninja_deps.log" >nul
if errorlevel 1 (
    echo repaired a corrupt %BUILDDIR%\.ninja_deps ^(this build recompiles everything once^)
) else (
    echo %BUILDDIR%\.ninja_deps did not survive a recompact -- removing it
    del /q "%BUILDDIR%\.ninja_deps"
)
:deps_checked
del /q "%TEMP%\ss_ninja_deps.log" >nul 2>&1
:deps_done

echo.
set JOB_RAM_MB=750
set AVAILABLE_MB=0
for /f %%m in ('powershell -NoProfile -Command "[int]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024)"') do set AVAILABLE_MB=%%m
set /a JOBS=AVAILABLE_MB/JOB_RAM_MB
if %JOBS% gtr %NUMBER_OF_PROCESSORS% set JOBS=%NUMBER_OF_PROCESSORS%
if %JOBS% lss 1 set JOBS=1
echo Build dir     : %BUILDDIR%
echo Available RAM : %AVAILABLE_MB% MB
echo CPU cores     : %NUMBER_OF_PROCESSORS%
echo Using jobs    : %JOBS%
echo.

rem `if errorlevel 1` is a >= test on a SIGNED value, so it reads a negative
rem exit code as success -- a failed link returning -1 printed "Build complete".
set "BUILDCMD=cmake --build "%BUILDDIR%" -j %JOBS%"
if "%WARNMODE%"=="at-end" set "BUILDCMD=python tools\check_build_warnings.py -- %BUILDCMD%"
%BUILDCMD%
if %ERRORLEVEL% equ 0 goto :built
if "%WERROR%"=="ON" echo If a compiler warning stopped it, rerun with --allow-warnings and report the warning.
exit /b 1
:built

echo.
echo Build complete: %BUILDDIR%\spirula.exe

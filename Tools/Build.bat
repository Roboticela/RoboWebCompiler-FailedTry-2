@echo off
rem ---------------------------------------------------------------------------
rem RoboHTMLCompiler build driver for Windows.
rem
rem Locates a Visual Studio installation with the C++ tools, imports the native
rem x64 environment and drives CMake with the Ninja generator so that the build
rem tree layout stays single-configuration and predictable:
rem
rem     build/GeneratedHTML/index.html
rem
rem Usage:
rem     Tools\Build.bat [Debug|Release|RelWithDebInfo|MinSizeRel] [clean]
rem ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

set "RoboBuildType=%~1"
if "%RoboBuildType%"=="" set "RoboBuildType=Release"

set "RoboRoot=%~dp0.."
for %%I in ("%RoboRoot%") do set "RoboRoot=%%~fI"
set "RoboBuildDirectory=%RoboRoot%\build"

call :FindVisualStudio

if not defined VsInstall (
    echo [RoboHTMLCompiler] No Visual Studio installation with the C++ tools was found.
    echo [RoboHTMLCompiler] Install the "Desktop development with C++" workload, or set VSINSTALLDIR.
    exit /b 1
)

echo [RoboHTMLCompiler] using %VsInstall%
call "%VsInstall%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [RoboHTMLCompiler] Failed to initialise the MSVC x64 environment.
    exit /b 1
)

call :FindTool RoboCmake cmake.exe "%VsInstall%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined RoboCmake (
    echo [RoboHTMLCompiler] cmake.exe was not found on PATH or in Visual Studio.
    exit /b 1
)

call :FindTool RoboNinja ninja.exe "%VsInstall%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not defined RoboNinja (
    echo [RoboHTMLCompiler] ninja.exe was not found on PATH or in Visual Studio.
    exit /b 1
)

if /i "%~2"=="clean" (
    if exist "%RoboBuildDirectory%" rmdir /s /q "%RoboBuildDirectory%"
)

echo [RoboHTMLCompiler] configuring %RoboBuildType% with Ninja
"%RoboCmake%" -S "%RoboRoot%" -B "%RoboBuildDirectory%" -G Ninja ^
    -DCMAKE_MAKE_PROGRAM="%RoboNinja%" ^
    -DCMAKE_BUILD_TYPE=%RoboBuildType%
if errorlevel 1 exit /b 1

echo [RoboHTMLCompiler] building
"%RoboCmake%" --build "%RoboBuildDirectory%" --config %RoboBuildType%
if errorlevel 1 exit /b 1

echo [RoboHTMLCompiler] build complete: %RoboBuildDirectory%\bin
endlocal
exit /b 0

rem ---------------------------------------------------------------------------
rem :FindVisualStudio - sets VsInstall to an installation with vcvars64.bat
rem ---------------------------------------------------------------------------
:FindVisualStudio
set "VsInstall="

if defined VSINSTALLDIR (
    if exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat" (
        set "VsInstall=%VSINSTALLDIR%"
        exit /b 0
    )
)

set "VsWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VsWhere%" (
    for /f "usebackq tokens=*" %%I in (`"%VsWhere%" -latest -prerelease -products * -property installationPath`) do (
        if not defined VsInstall if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "VsInstall=%%I"
    )
)

if defined VsInstall exit /b 0

set "VsCandidates=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise;%ProgramFiles%\Microsoft Visual Studio\2022\Professional;%ProgramFiles%\Microsoft Visual Studio\2022\Community;%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools;%ProgramFiles%\Microsoft Visual Studio\2022\Preview;%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools;%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community;%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools;%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
for %%C in ("%VsCandidates:;=" "%") do (
    if not defined VsInstall if exist "%%~C\VC\Auxiliary\Build\vcvars64.bat" set "VsInstall=%%~C"
)

exit /b 0

rem ---------------------------------------------------------------------------
rem :FindTool <variableName> <executableName> <fallbackPath>
rem ---------------------------------------------------------------------------
:FindTool
set "%~1="
for %%I in (%~2) do if not defined %~1 set "%~1=%%~$PATH:I"
if not defined %~1 if exist "%~3" set "%~1=%~3"
exit /b 0

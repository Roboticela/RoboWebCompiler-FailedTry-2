@echo off
rem ---------------------------------------------------------------------------
rem RoboPlay driver for Windows.
rem
rem     Tools\Play.bat                 pick an HTML file (and a CSS file) and play
rem     Tools\Play.bat page.html       play a document, its own stylesheet found
rem     Tools\Play.bat page.html app.css
rem
rem RoboPlay opens the document in a window and forwards the mouse, the wheel,
rem the keyboard and the resize to the runtime, so a hover, a focused field and
rem a resize behave the way they do in a browser. This driver finds the player,
rem builds it the first time, asks for the two files when they were not given,
rem and starts it.
rem
rem Options after the two files go to RoboPlay:
rem     --width N  --height N  --assets DIR  --watch  --debug  --title TEXT
rem
rem Pass --build to rebuild the player even when the executable already exists,
rem or --build-only to rebuild it and stop.
rem
rem The file picker is the Windows one, through PowerShell, so it needs no extra
rem tooling. Cancel the CSS dialog to let the document's own stylesheet be used.
rem ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

set "RoboRoot=%~dp0.."
for %%I in ("%RoboRoot%") do set "RoboRoot=%%~fI"
set "RoboBuildDirectory=%RoboRoot%\build"
set "RoboExe=%RoboBuildDirectory%\bin\RoboPlay.exe"

set "RoboHtml="
set "RoboStyle="
set "RoboOptions="
set "RoboRebuild=0"
set "RoboBuildOnly=0"

rem The first non-switch argument is the document, the second the stylesheet;
rem anything that starts with a dash is passed on to the player.
:ParseArguments
if "%~1"=="" goto :ArgumentsDone

if /i "%~1"=="--help" goto :usage
if /i "%~1"=="-h" goto :usage
if "%~1"=="/?" goto :usage

if /i "%~1"=="--build" (
    set "RoboRebuild=1"
    shift
    goto :ParseArguments
)

if /i "%~1"=="--build-only" (
    set "RoboRebuild=1"
    set "RoboBuildOnly=1"
    shift
    goto :ParseArguments
)

set "RoboArgument=%~1"

rem A path that ends in a backslash cannot be quoted on a Windows command line:
rem the backslash escapes the closing quote and the path swallows what follows.
if "!RoboArgument:~-1!"=="\" set "RoboArgument=!RoboArgument:~0,-1!"

if "!RoboArgument:~0,1!"=="-" (
    set "RoboOptions=!RoboOptions! !RoboArgument!"

    if not "%~2"=="" (
        set "RoboNext=%~2"
        if not "!RoboNext:~0,1!"=="-" (
            set "RoboOptions=!RoboOptions! !RoboNext!"
            shift
        )
    )

    shift
    goto :ParseArguments
)

if "%RoboHtml%"=="" (
    for %%I in ("!RoboArgument!") do set "RoboHtml=%%~fI"
) else if "%RoboStyle%"=="" (
    for %%I in ("!RoboArgument!") do set "RoboStyle=%%~fI"
) else (
    echo [RoboPlay] already given a document and a stylesheet; this is neither: %~1
    echo.
    goto :usage
)

shift
goto :ParseArguments

:ArgumentsDone

rem --build-only is about the executable, not a document, so it is answered
rem before any file is asked for.
if "%RoboBuildOnly%"=="1" (
    call :BuildTarget
    if errorlevel 1 (
        echo [RoboPlay] the player could not be built.
        echo [RoboPlay] if the build tree is not configured yet, run:  Tools\Build.bat
        endlocal
        exit /b 1
    )

    echo [RoboPlay] built %RoboExe%
    endlocal
    exit /b 0
)

rem --- ask for the two files when they were not given ------------------------

if "%RoboHtml%"=="" (
    call :PickFile "HTML files (*.html;*.htm)|*.html;*.htm|All files (*.*)|*.*" "Pick an HTML document" RoboHtml
    if not defined RoboHtml (
        echo [RoboPlay] no document chosen.
        endlocal
        exit /b 1
    )
)

if not exist "%RoboHtml%" (
    echo [RoboPlay] no such document: %RoboHtml%
    endlocal
    exit /b 1
)

if "%RoboStyle%"=="" (
    call :PickFile "CSS files (*.css)|*.css|All files (*.*)|*.*" "Pick a stylesheet (Cancel to use the document stylesheet)" RoboStyle
)

rem --- build the player ------------------------------------------------------

if "%RoboRebuild%"=="1" goto :Build
if not exist "%RoboExe%" goto :Build
goto :Built

:Build
call :BuildTarget
if errorlevel 1 (
    echo [RoboPlay] the player could not be built.
    echo [RoboPlay] if the build tree is not configured yet, run:  Tools\Build.bat
    endlocal
    exit /b 1
)

:Built

if not exist "%RoboExe%" (
    echo [RoboPlay] the player is not there: %RoboExe%
    echo [RoboPlay] build it with:  Tools\Build.bat
    endlocal
    exit /b 1
)

if "%RoboBuildOnly%"=="1" (
    echo [RoboPlay] built %RoboExe%
    endlocal
    exit /b 0
)

rem --- run --------------------------------------------------------------------

set "RoboArguments="
if not "%RoboStyle%"=="" set "RoboArguments= --css "%RoboStyle%""

echo [RoboPlay] document:   %RoboHtml%
if not "%RoboStyle%"=="" echo [RoboPlay] stylesheet: %RoboStyle%
echo [RoboPlay] F5 reloads, Escape closes.

start "RoboPlay" "%RoboExe%" "%RoboHtml%"%RoboArguments%%RoboOptions%

endlocal
exit /b 0

rem ---------------------------------------------------------------------------
rem :PickFile <filter> <title> <variableName>
rem Opens the Windows file picker through PowerShell and stores the result in
rem the named variable. Nothing is stored when the dialog is cancelled.
:PickFile
set "%~3="
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "Add-Type -AssemblyName System.Windows.Forms; $d = New-Object System.Windows.Forms.OpenFileDialog; $d.Filter = '%~1'; $d.Title = '%~2'; $d.InitialDirectory = '%RoboRoot%\Examples'; if ($d.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $d.FileName }"`) do set "%~3=%%I"
exit /b 0

rem ---------------------------------------------------------------------------
rem :BuildTarget
rem Builds only the RoboPlay target, through the same Visual Studio toolchain the
rem build driver uses, so the full build is not disturbed by another process
rem holding one of the other executables open.
:BuildTarget
set "VsInstall="
if defined VSINSTALLDIR if exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat" set "VsInstall=%VSINSTALLDIR%"

rem The variable name `ProgramFiles(x86)` contains a bracket, so it is read here
rem at the top level rather than inside a parenthesised block: there it would end
rem the block early.
set "RoboVsWhere="
if defined ProgramFiles(x86) set "RoboVsWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not defined VsInstall if defined RoboVsWhere if exist "%RoboVsWhere%" (
    for /f "usebackq tokens=*" %%I in (`"%RoboVsWhere%" -latest -prerelease -products * -property installationPath`) do (
        if not defined VsInstall if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "VsInstall=%%I"
    )
)

rem vswhere is not always installed on a machine that still has the tools, so
rem the usual locations are tried directly, the way the build driver does.
set "RoboVsCandidates=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise;%ProgramFiles%\Microsoft Visual Studio\2022\Professional;%ProgramFiles%\Microsoft Visual Studio\2022\Community;%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools;%ProgramFiles%\Microsoft Visual Studio\2022\Preview"

if not defined VsInstall (
    for %%C in ("%RoboVsCandidates:;=" "%") do (
        if not defined VsInstall if exist "%%~C\VC\Auxiliary\Build\vcvars64.bat" set "VsInstall=%%~C"
    )
)

if not defined VsInstall (
    echo [RoboPlay] no Visual Studio with the C++ tools was found.
    exit /b 1
)

call "%VsInstall%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 exit /b 1

set "RoboNinja=%VsInstall%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%RoboNinja%" set "RoboNinja=ninja.exe"

"%RoboNinja%" -C "%RoboBuildDirectory%" RoboPlay
if errorlevel 1 exit /b 1

exit /b 0

:usage
echo RoboPlay driver
echo.
echo Usage:
echo     Tools\Play.bat [document.html] [stylesheet.css] [RoboPlay options]
echo.
echo With no arguments it asks for an HTML file and then a CSS file. Cancel the
echo CSS dialog to let the document's own stylesheet be used.
echo.
echo Options are passed to RoboPlay:
echo     --width N  --height N  --assets DIR  --watch  --debug  --title TEXT
echo.
echo Pass --build to rebuild the player first, or --build-only to rebuild and stop.
echo.
echo Examples:
echo     Tools\Play.bat
echo     Tools\Play.bat Examples\Conformance\index.html
echo     Tools\Play.bat Examples\LandingPage\index.html Examples\LandingPage\app.css
echo     Tools\Play.bat Examples\Watch\index.html --width 1200 --height 800 --watch
endlocal
exit /b 2

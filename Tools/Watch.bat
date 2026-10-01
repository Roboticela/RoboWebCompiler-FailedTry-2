@echo off
rem ---------------------------------------------------------------------------
rem RoboHTMLCompiler live preview driver for Windows.
rem
rem     Tools\Run.bat     compiles a folder of documents into generated C++
rem     Tools\Watch.bat   previews one document while it is being written
rem
rem The code editor is opened on the left half of the screen and the preview on
rem the right. The preview holds the HTML in one box and the stylesheet in the
rem other, shows the frame they compile to and writes it to preview.png beside
rem the document. The boxes compile when they are asked to - F5, the Rebuild
rem button, or Ctrl+S, which writes them back to the two files first - so typing
rem never starts a build and a large document never stalls the window. Pass
rem --auto to have the window keep up on its own instead: a change in a box or in
rem either file is then rebuilt. Editing in the editor and editing in the boxes
rem are the same loop either way, because a file saved on disk is reloaded into
rem its box.
rem
rem Usage:
rem     Tools\Watch.bat [document.html] [stylesheet.css] [RoboWatch options]
rem
rem Examples:
rem     Tools\Watch.bat
rem     Tools\Watch.bat Examples\Watch\index.html
rem     Tools\Watch.bat Examples\Watch\typography.html
rem     Tools\Watch.bat Examples\LandingPage\index.html Examples\LandingPage\app.css
rem     Tools\Watch.bat Examples\LandingPage\index.html --width 1200 --height 800
rem     Tools\Watch.bat Examples\Watch\index.html --auto
rem
rem The stylesheet is the one the document links; failing that, `app.css` beside
rem it. `Examples\Watch\typography.html` links `typography.css` and puts every
rem font and text property on one page, which is what makes it the quickest way
rem to see the text engine's own work.
rem
rem The editor is the first one of `code`, `notepad++` and `notepad` that is on
rem the path; set ROBO_EDITOR to choose another one.
rem
rem Set ROBO_WATCH_CONSOLE to run the preview in this console rather than in a
rem window of its own. That is for scripting - with `--once` it turns the driver
rem into "build this page and write its PNG" with no window in the way.
rem ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

set "RoboRoot=%~dp0.."
for %%I in ("%RoboRoot%") do set "RoboRoot=%%~fI"

rem Both builds are looked for and the newer executable is the one used. A
rem checkout is usually one of the two, but it can hold both, and after a
rem rebuild one of them is a revision behind. A preview that draws yesterday's
rem text engine is worse than no preview at all, so the choice is made by
rem modification time rather than by a fixed order.
set "RoboExe="

for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "Get-Item -ErrorAction SilentlyContinue '%RoboRoot%\build\bin\RoboWatch.exe','%RoboRoot%\build-clang\bin\RoboWatch.exe' | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName"`) do set "RoboExe=%%I"

rem PowerShell is always present on the systems this driver supports, but if it
rem somehow is not, the preset build and then the CMake build are the order the
rem driver used before.
if not defined RoboExe (
    set "RoboExe=%RoboRoot%\build-clang\bin\RoboWatch.exe"
    if not exist "%RoboExe%" set "RoboExe=%RoboRoot%\build\bin\RoboWatch.exe"
)

set "RoboDocument="
set "RoboStyleSheet="
set "RoboOptions="

:ParseArguments
if "%~1"=="" goto :ArgumentsDone

if /i "%~1"=="--help" goto :usage
if /i "%~1"=="-h" goto :usage
if "%~1"=="/?" goto :usage

set "RoboArgument=%~1"

rem A path that ends in a backslash cannot be quoted on a Windows command line:
rem the backslash escapes the closing quote and the path swallows what follows
rem it. Windows accepts `Examples\Watch\` on the command line, so the trailing
rem backslash is removed here, before the value is ever quoted.
if "!RoboArgument:~-1!"=="\" set "RoboArgument=!RoboArgument:~0,-1!"

rem Anything that starts with a dash is passed straight through to RoboWatch:
rem its options are its own, and this driver does not need to know them one by
rem one. The token after a switch is taken as its value unless it is another
rem switch, because a path never starts with a dash: `--width 1200` is one option
rem and one value, while `--quiet` is an option on its own.
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

if "%RoboDocument%"=="" (
    for %%I in ("!RoboArgument!") do set "RoboDocument=%%~fI"
) else if "%RoboStyleSheet%"=="" (
    for %%I in ("!RoboArgument!") do set "RoboStyleSheet=%%~fI"
) else (
    echo [RoboWatch] already given a document and a stylesheet; this is neither: %~1
    echo.
    goto :usage
)

shift
goto :ParseArguments

:ArgumentsDone

if "%RoboDocument%"=="" set "RoboDocument=%RoboRoot%\Examples\Watch\index.html"

if not exist "%RoboExe%" (
    echo [RoboWatch] the preview is not built yet; building it first
    call "%RoboRoot%\Tools\Build.bat"
    if errorlevel 1 exit /b 1
)

if not exist "%RoboExe%" (
    echo [RoboWatch] the preview still is not there. Build it with:
    echo     Tools\Build.bat
    echo or configure the clang preset:
    echo     cmake --preset clang ^&^& cmake --build build-clang --target RoboWatch
    exit /b 1
)

if not exist "%RoboDocument%" (
    echo [RoboWatch] no such document: %RoboDocument%
    exit /b 1
)

rem The arguments are built up rather than the whole command, so that the one
rem line that differs between the two ways of starting the preview is the line
rem that starts it.
set "RoboArguments="

if not "%RoboStyleSheet%"=="" set "RoboArguments=%RoboArguments% --css "%RoboStyleSheet%""
if defined ROBO_EDITOR set "RoboArguments=%RoboArguments% --editor "%ROBO_EDITOR%""
if not "%RoboOptions%"=="" set "RoboArguments=%RoboArguments%%RoboOptions%"

echo [RoboWatch] document:   %RoboDocument%
if not "%RoboStyleSheet%"=="" echo [RoboWatch] stylesheet: %RoboStyleSheet%
echo [RoboWatch] editor on the left, live preview on the right.

rem The preview owns its console window when it is started normally: it logs
rem every build into it, and closing the window stops the watch.
if defined ROBO_WATCH_CONSOLE (
    "%RoboExe%" "%RoboDocument%" %RoboArguments%
    if errorlevel 1 exit /b 1
    endlocal
    exit /b 0
)

start "RoboWatch" "%RoboExe%" "%RoboDocument%" %RoboArguments%

endlocal
exit /b 0

:usage
echo RoboWatch live preview driver
echo.
echo Usage:
echo     Tools\Watch.bat [document.html] [stylesheet.css] [RoboWatch options]
echo.
echo Opens the code editor on the left half of the screen and the live preview on
echo the right: the HTML box, the CSS box and the frame they compile to, with the
echo frame written to preview.png. The boxes compile when F5, the Rebuild button
echo or Ctrl+S asks for it; pass --auto to rebuild as they are typed in instead.
echo.
echo With no arguments it previews Examples\Watch\index.html.
echo.
echo Options are passed to RoboWatch; run
echo     RoboWatch.exe --help
echo for the list.
echo.
echo Examples:
echo     Tools\Watch.bat
echo     Tools\Watch.bat Examples\LandingPage\index.html
echo     Tools\Watch.bat Examples\Watch\typography.html
echo     Tools\Watch.bat Examples\LandingPage\index.html --width 1200 --height 800
echo     Tools\Watch.bat Examples\Watch\index.html --auto
exit /b 2

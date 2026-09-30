@echo off
rem ---------------------------------------------------------------------------
rem RoboHTMLCompiler run driver for Windows.
rem
rem Compiles every HTML document in an input folder into its own strongly typed
rem C++ native model, keeping the generated browser output and IR alongside it:
rem
rem     <output>/<DocumentName>/GeneratedHTML/<document>.html
rem     <output>/<DocumentName>/GeneratedNative/<DocumentName>NativeModel.{h,cpp}
rem     <output>/<DocumentName>/GeneratedIR/<DocumentName>.ir.json
rem     <output>/<DocumentName>/GeneratedReports/
rem     <output>/BatchReport.json
rem
rem Usage:
rem     Tools\Run.bat <inputDirectory> [outputDirectory] [buildType]
rem     Tools\Run.bat <inputDirectory> --output <outputDirectory>
rem
rem Examples:
rem     Tools\Run.bat Examples\MultiPage
rem     Tools\Run.bat Examples\MultiPage build\compiled\MultiPage Release
rem     Tools\Run.bat Examples\MultiPage --output build\compiled\MultiPage
rem ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

rem The script's own location is read first, before the argument loop below: in a
rem batch file `shift` moves %0 as well, so a root computed after a shift would
rem be the path of whichever argument ended up in %0.
set "RoboRoot=%~dp0.."
for %%I in ("%RoboRoot%") do set "RoboRoot=%%~fI"
set "RoboBuildDirectory=%RoboRoot%\build"
set "RoboExe=%RoboBuildDirectory%\bin\RoboHTMLCompiler.exe"

set "RoboInput="
set "RoboOutput="
set "RoboBuildType=Release"

rem Arguments are read one at a time so that both forms work: the positional
rem form the tools have always used, and the --output form a reader is likely to
rem try first. Anything else is a mistake worth naming.
:ParseArguments
if "%~1"=="" goto :ArgumentsDone

if /i "%~1"=="--output" goto :OutputOption
if /i "%~1"=="-o" goto :OutputOption
if /i "%~1"=="--build" goto :BuildOption
if /i "%~1"=="--build-type" goto :BuildOption
if /i "%~1"=="--help" goto :usage
if /i "%~1"=="-h" goto :usage

rem A switch that is not one of the above is not a path, and treating it as one
rem would produce a "directory not found" message about a flag.
if "%~1"=="/?" goto :usage
set "RoboArgument=%~1"
if "!RoboArgument:~0,1!"=="-" (
    echo [RoboHTMLCompiler] unknown option: %~1
    echo.
    goto :usage
)

rem A path that ends in a backslash cannot be quoted on a Windows command line:
rem the backslash escapes the closing quote and the path swallows the arguments
rem after it. Windows accepts `Examples\MultiPage\` on the command line, so the
rem trailing backslash is removed here, before the value is ever quoted.
if "!RoboArgument:~-1!"=="\" set "RoboArgument=!RoboArgument:~0,-1!"

if "%RoboInput%"=="" (
    for %%I in ("!RoboArgument!") do set "RoboInput=%%~fI"
) else if "%RoboOutput%"=="" (
    for %%I in ("!RoboArgument!") do set "RoboOutput=%%~fI"
) else (
    set "RoboBuildType=!RoboArgument!"
)

shift
goto :ParseArguments

:OutputOption
if "%~2"=="" (
    echo [RoboHTMLCompiler] --output needs a directory, for example --output build\compiled\App
    exit /b 2
)

set "RoboArgument=%~2"
if "!RoboArgument:~-1!"=="\" set "RoboArgument=!RoboArgument:~0,-1!"
for %%I in ("!RoboArgument!") do set "RoboOutput=%%~fI"
shift
shift
goto :ParseArguments

:BuildOption
if "%~2"=="" (
    echo [RoboHTMLCompiler] --build needs Debug, Release, RelWithDebInfo or MinSizeRel
    exit /b 2
)
set "RoboBuildType=%~2"
shift
shift
goto :ParseArguments

:ArgumentsDone

if "%RoboInput%"=="" goto :usage

rem `for %%~f` puts a backslash back on a path that names a directory, and a path
rem that ends in a backslash cannot be quoted on a Windows command line: the
rem backslash escapes the closing quote and the path swallows the arguments after
rem it. The trailing backslashes come off here, after the last thing that could
rem add one.
call :StripTrailingBackslashes RoboInput
call :StripTrailingBackslashes RoboOutput

rem The path is normalised with %~f1 before it is used, so a path that the user
rem wrote with a trailing backslash cannot escape the quotes around it when the
rem compiler is started.
if not exist "%RoboInput%" (
    echo [RoboHTMLCompiler] input directory not found: %RoboInput%
    echo [RoboHTMLCompiler] give the folder that holds the .html, .css and .js files.
    exit /b 1
)

if not exist "%RoboInput%\*" (
    echo [RoboHTMLCompiler] this is a file, not a folder: %RoboInput%
    echo [RoboHTMLCompiler] this driver compiles a whole folder; give the folder that contains it.
    exit /b 1
)

if not exist "%RoboExe%" (
    echo [RoboHTMLCompiler] the compiler is not built yet; building it first
    call "%RoboRoot%\Tools\Build.bat" %RoboBuildType%
    if errorlevel 1 exit /b 1
)

if "%RoboOutput%"=="" (
    for %%I in ("%RoboInput%") do set "RoboLeaf=%%~nxI"
    if "!RoboLeaf!"=="" set "RoboLeaf=compiled"
    set "RoboOutput=%RoboBuildDirectory%\compiled\!RoboLeaf!"
)

call :StripTrailingBackslashes RoboOutput

echo [RoboHTMLCompiler] input:  %RoboInput%
echo [RoboHTMLCompiler] output: %RoboOutput%
echo. 

"%RoboExe%" "%RoboInput%" --output "%RoboOutput%" --all
if errorlevel 1 (
    echo.
    echo [RoboHTMLCompiler] run failed: see the errors above, one for each document that did not compile.
    exit /b 1
)

echo.
echo [RoboHTMLCompiler] done; open %RoboOutput% to inspect the generated C++.
endlocal
exit /b 0

rem ---------------------------------------------------------------------------
rem :StripTrailingBackslashes <variableName>
rem Removes trailing backslashes from the named variable, keeping the root of a
rem drive (`C:\`) intact. Used on every path before it is passed on.
:StripTrailingBackslashes
if not defined %~1 exit /b 0
set "RoboValue=!%~1!"
:StripTrailingBackslashesLoop
if not "!RoboValue:~-1!"=="\" goto :StripTrailingBackslashesDone
if "!RoboValue:~-2,1!"==":" goto :StripTrailingBackslashesDone
set "RoboValue=!RoboValue:~0,-1!"
goto :StripTrailingBackslashesLoop
:StripTrailingBackslashesDone
set "%~1=!RoboValue!"
exit /b 0

:usage
echo RoboHTMLCompiler run driver
echo.
echo Usage:
echo     Tools\Run.bat ^<inputDirectory^> [outputDirectory] [buildType]
echo     Tools\Run.bat ^<inputDirectory^> --output ^<outputDirectory^>
echo.
echo Compiles every .html document in the input directory into native C++,
echo one subdirectory per document, plus a BatchReport.json summary.
echo.
echo Example:
echo     Tools\Run.bat Examples\MultiPage
exit /b 2

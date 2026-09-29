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
rem
rem Examples:
rem     Tools\Run.bat Examples\MultiPage
rem     Tools\Run.bat Examples\MultiPage build\compiled\MultiPage Release
rem ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

set "RoboInput=%~1"
set "RoboOutput=%~2"
set "RoboBuildType=%~3"
if "%RoboBuildType%"=="" set "RoboBuildType=Release"

if "%RoboInput%"=="" goto :usage

set "RoboRoot=%~dp0.."
for %%I in ("%RoboRoot%") do set "RoboRoot=%%~fI"
set "RoboBuildDirectory=%RoboRoot%\build"
set "RoboExe=%RoboBuildDirectory%\bin\RoboHTMLCompiler.exe"

if not exist "%RoboInput%" (
    echo [RoboHTMLCompiler] input directory not found: %RoboInput%
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

echo [RoboHTMLCompiler] input:  %RoboInput%
echo [RoboHTMLCompiler] output: %RoboOutput%
echo. 

"%RoboExe%" "%RoboInput%" --output "%RoboOutput%" --all
if errorlevel 1 (
    echo.
    echo [RoboHTMLCompiler] run failed: at least one document did not compile.
    exit /b 1
)

echo.
echo [RoboHTMLCompiler] done; open %RoboOutput% to inspect the generated C++.
endlocal
exit /b 0

:usage
echo RoboHTMLCompiler run driver
echo.
echo Usage:
echo     Tools\Run.bat ^<inputDirectory^> [outputDirectory] [buildType]
echo.
echo Compiles every .html document in the input directory into native C++,
echo one subdirectory per document, plus a BatchReport.json summary.
echo.
echo Example:
echo     Tools\Run.bat Examples\MultiPage
exit /b 2

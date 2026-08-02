@echo off
REM build_iso.bat — Stage disc tree and pack pvz_wii.iso / pvz_wii.wbfs with WIT
REM Usage: scripts\build_iso.bat [path\to\game.dol]
REM WIT does not need to be on PATH — see the WIT_PATH section below.
setlocal EnableDelayedExpansion

set "ROOT=%~dp0.."
pushd "%ROOT%" || exit /b 1

REM ---------------------------------------------------------------------------
REM WIT (Wiimms ISO Tools) location
REM
REM WIT does NOT need to be on your system PATH. WIT_PATH is normally set for
REM you by `make iso`. If you're running this script directly instead, either:
REM
REM   - copy wit.exe into tools\wit\ inside the project root:
REM       tools\wit\wit.exe
REM     it will be picked up automatically below, or
REM   - set WIT_PATH to point at your existing install before running this
REM     script:
REM       set "WIT_PATH=C:\path\to\wit.exe"
REM       scripts\build_iso.bat
REM
REM Download WIT: https://wit.wiimm.de/
REM ---------------------------------------------------------------------------
if not defined WIT_PATH set "WIT_PATH=%ROOT%\tools\wit\wit.exe"
if not exist "%WIT_PATH%" if exist "%ROOT%\tools\wit\wit" set "WIT_PATH=%ROOT%\tools\wit\wit"

set "TARGET=pvz_wii"
if not "%~1"=="" (
    set "DOL=%~1"
) else (
    set "DOL=%ROOT%\%TARGET%.dol"
)

set "META=%ROOT%\disc\meta\disc.info"
set "STAGING=%ROOT%\disc\staging"
set "HB_OUT=%ROOT%\output\homebrew"
set "ISO_OUT=%ROOT%\output\disc"
set "ASSETS=%ROOT%\disc\assets"

if not exist "%DOL%" (
    echo error: DOL not found: %DOL% ^(run 'make' first^)
    popd
    exit /b 1
)

if not exist "%META%" (
    echo error: missing %META%
    popd
    exit /b 1
)

if not exist "%WIT_PATH%" (
    echo error: 'wit' executable not found at: %WIT_PATH%
    echo        Copy Wiimms ISO Tools into tools\wit\ inside the project
    echo        ^(as wit.exe^), or set WIT_PATH to point at your install:
    echo          set "WIT_PATH=C:\path\to\wit.exe"
    echo        Download WIT: https://wit.wiimm.de/
    popd
    exit /b 1
)
echo ==^> Using wit: %WIT_PATH%

REM Parse disc.info (simple KEY=VALUE)
set "DISC_TITLE=Plants vs Zombies Wii"
set "DISC_ID=PVZW"
set "DISC_REGION=E"
for /f "usebackq tokens=1,* delims==" %%A in ("%META%") do (
    set "LINE=%%A"
    if not "!LINE:~0,1!"=="#" (
        if "%%A"=="DISC_TITLE" set "DISC_TITLE=%%B"
        if "%%A"=="DISC_ID" set "DISC_ID=%%B"
        if "%%A"=="DISC_REGION" set "DISC_REGION=%%B"
    )
)

echo ==^> Staging disc tree at %STAGING%
if exist "%STAGING%" rmdir /s /q "%STAGING%"
mkdir "%STAGING%\sys" 2>nul
mkdir "%STAGING%\files" 2>nul
mkdir "%HB_OUT%" 2>nul
mkdir "%ISO_OUT%" 2>nul

copy /y "%DOL%" "%STAGING%\sys\main.dol" >nul

if exist "%ASSETS%\files" (
    xcopy /e /i /y "%ASSETS%\files\*" "%STAGING%\files\" >nul
)

if exist "%ROOT%\assets" (
    mkdir "%STAGING%\files\assets" 2>nul
    xcopy /e /i /y "%ROOT%\assets\*" "%STAGING%\files\assets\" >nul
)

if exist "%ASSETS%\opening.bnr" (
    copy /y "%ASSETS%\opening.bnr" "%STAGING%\opening.bnr" >nul
    echo     included opening.bnr
) else (
    echo     note: disc\assets\opening.bnr not found ^(optional banner skipped^)
)

copy /y "%DOL%" "%HB_OUT%\boot.dol" >nul
copy /y "%ROOT%\meta.xml" "%HB_OUT%\meta.xml" >nul
if exist "%ROOT%\icon.png" copy /y "%ROOT%\icon.png" "%HB_OUT%\icon.png" >nul
if exist "%ROOT%\assets" (
    mkdir "%HB_OUT%\assets" 2>nul
    xcopy /e /i /y "%ROOT%\assets\*" "%HB_OUT%\assets\" >nul
)

set "ISO_FILE=%ISO_OUT%\%TARGET%.iso"
set "WBFS_FILE=%ISO_OUT%\%TARGET%.wbfs"

echo ==^> Building WBFS: %WBFS_FILE%
"%WIT_PATH%" copy "%STAGING%/" "%WBFS_FILE%" --overwrite --name "%DISC_TITLE%" --id %DISC_ID% --region %DISC_REGION%
if errorlevel 1 goto :wit_fail

echo ==^> Building ISO:  %ISO_FILE%
"%WIT_PATH%" copy "%STAGING%/" "%ISO_FILE%" --iso --overwrite --name "%DISC_TITLE%" --id %DISC_ID% --region %DISC_REGION%
if errorlevel 1 goto :wit_fail

echo.
echo Done.
echo   Homebrew:  %HB_OUT%\boot.dol
echo   ISO:       %ISO_FILE%
echo   WBFS:      %WBFS_FILE%
echo.
echo Open in Dolphin: File -^> Open -^> %ISO_FILE%
popd
exit /b 0

:wit_fail
echo error: wit command failed
popd
exit /b 1

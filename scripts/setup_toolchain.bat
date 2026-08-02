@echo off
rem Double-click this to fetch the whole build toolchain into tools/.
rem Just runs setup_toolchain.ps1 -- see that file for what actually happens.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_toolchain.ps1"
if errorlevel 1 (
    echo.
    echo Setup failed -- see the error above.
    pause
    exit /b 1
)
pause

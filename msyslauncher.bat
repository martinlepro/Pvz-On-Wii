@echo off
rem Opens an MSYS2 shell using this project's own vendored devkitPro copy.
rem Fully relative to this file's location -- no absolute paths, no env vars.
setlocal
set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"

if not exist "%ROOT%\tools\devkitpro\msys2\usr\bin\bash.exe" (
    echo devkitPro toolchain not found in tools\devkitpro\
    echo Run scripts\setup_toolchain.bat first to fetch it automatically.
    pause
    exit /b 1
)

set "HOME=%ROOT%\tools\devkitpro\msys2\home"
"%ROOT%\tools\devkitpro\msys2\usr\bin\bash.exe" --login -i

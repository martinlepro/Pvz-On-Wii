# One-time setup for this project.
#
# Builds a portable devkitPPC (Wii) toolchain, builds GRRLIB, and fetches
# Wiimms ISO Tools -- all entirely into tools/devkitpro and tools/wit next to
# this repo. Nothing is installed system-wide, no installer GUI is used.
#
# Everything this script can legally redistribute is already bundled in the
# vendor/ folder next to this repo (MSYS2 base + GRRLIB source), so those two
# steps need no internet access and no reliance on machine-specific paths.
# devkitPPC/libogc/the Wii portlibs (installed via pacman from devkitPro's own
# package server) and Wiimms ISO Tools (from wit.wiimm.de) cannot legally be
# bundled here -- devkitPro distributes them only through pacman, and WIT only
# through Wiimm's own site -- so those two steps still need internet access to
# pkg.devkitpro.org and wit.wiimm.de respectively. If your network blocks
# those specific hosts, that's the next thing to check.
#
# All scratch/temp files are created under tools/_work (inside this project),
# never under $env:TEMP or any other machine-specific location. The only
# environment variable this script reads is $env:SystemRoot, to call the
# Windows-native tar.exe by its known, fixed path -- not searched for on
# PATH -- since a stray MSYS/Cygwin/Git-for-Windows "tar" earlier on PATH
# mishandles Windows drive letters (that was the "Cannot connect to C:" bug).
#
# Safe to re-run: each step is skipped if it's already done.
#
# Usage (from a normal PowerShell or by double-clicking setup_toolchain.bat):
#   powershell -ExecutionPolicy Bypass -File scripts\setup_toolchain.ps1

$ErrorActionPreference = "Stop"

$RootDir   = Split-Path -Parent $PSScriptRoot
$VendorDir = Join-Path $RootDir "vendor"
$ToolsDir  = Join-Path $RootDir "tools"
$WorkDir   = Join-Path $ToolsDir "_work"
$DkpDir    = Join-Path $ToolsDir "devkitpro"
$WitDir    = Join-Path $ToolsDir "wit"
$Msys2Dir  = Join-Path $DkpDir "msys2"
$BashExe   = Join-Path $Msys2Dir "usr\bin\bash.exe"
$TarExe    = Join-Path $env:SystemRoot "System32\tar.exe"

$VendoredMsys2   = Join-Path $VendorDir "msys2-base-x86_64.tar.xz"
$VendoredGrrlib  = Join-Path $VendorDir "grrlib-src"

# WIT version -- bump this if https://wit.wiimm.de/download.html lists a newer one.
$WitVersion = "v3.05a-r8638"
$WitUrl     = "https://wit.wiimm.de/download/wit-$WitVersion-cygwin64.zip"

function Write-Step($msg) {
    Write-Host ""
    Write-Host "==> $msg" -ForegroundColor Cyan
}

function Invoke-Bash($cmd) {
    & $BashExe --login -c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed (exit $LASTEXITCODE): $cmd"
    }
}

# Converts a Windows path to the /d/... style MSYS2 bash expects.
function To-MsysPath($winPath) {
    $p = $winPath -replace '\\', '/'
    if ($p -match '^([A-Za-z]):(.*)$') {
        return "/$($Matches[1].ToLower())$($Matches[2])"
    }
    return $p
}

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

if (-not (Test-Path $TarExe)) {
    throw "$TarExe not found (needs Windows 10 1803+ / Server 2019+). Update Windows, or ask for a 7-Zip-based fallback."
}

# ---------------------------------------------------------------------------
# Step 1: portable MSYS2 base (this becomes tools/devkitpro/msys2)
# ---------------------------------------------------------------------------
if (Test-Path (Join-Path $DkpDir "devkitPPC\wii_rules")) {
    Write-Step "devkitPPC already present in tools/devkitpro -- skipping toolchain install."
} else {
    if (-not (Test-Path $BashExe)) {
        New-Item -ItemType Directory -Force -Path $DkpDir | Out-Null

        if (Test-Path $VendoredMsys2) {
            Write-Step "Using bundled MSYS2 from vendor/msys2-base-x86_64.tar.xz (no download needed)..."
            $archive = $VendoredMsys2
            $deleteArchiveAfter = $false
        } else {
            Write-Step "vendor/msys2-base-x86_64.tar.xz not found -- downloading MSYS2 instead..."
            $archive = Join-Path $WorkDir "msys2-base.tar.xz"
            Invoke-WebRequest -Uri "https://github.com/msys2/msys2-installer/releases/latest/download/msys2-base-x86_64-latest.tar.xz" -OutFile $archive
            $deleteArchiveAfter = $true
        }

        Write-Step "Extracting MSYS2 into tools/devkitpro/msys2 (thousands of small files -- antivirus real-time scanning can make this take 10-20+ minutes)..."
        # The archive contains a top-level msys64/ folder; extract then flatten it.
        $extractTmp = Join-Path $WorkDir "msys2-extract"
        Remove-Item $extractTmp -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $extractTmp | Out-Null

        # Plain, direct invocation -- no pipe, no Start-Process. Piping
        # ("| Out-Host") makes tar fully-buffer its output (can print
        # nothing until the very end). Start-Process -NoNewWindow -PassThru
        # is known to sometimes hand back a process object whose HasExited
        # never flips true even though nothing is actually running, which
        # produces an infinite "0 files extracted" loop with no process in
        # Task Manager. A direct call has neither problem: tar inherits this
        # console's handles directly, so -v's filenames print live as they
        # happen, and $LASTEXITCODE is always reliable.
        & $TarExe -xvf $archive -C $extractTmp
        if ($LASTEXITCODE -ne 0) {
            throw "tar.exe failed to extract $archive (exit $LASTEXITCODE)."
        }
        if ($deleteArchiveAfter) { Remove-Item $archive }
        Move-Item (Join-Path $extractTmp "msys64") $Msys2Dir
        Remove-Item $extractTmp -Recurse -Force -ErrorAction SilentlyContinue

        Write-Step "Bootstrapping MSYS2 (first run)..."
        Invoke-Bash "exit"
        # MSYS2's own docs: run pacman -Syu twice on a fresh install (it
        # restarts itself mid-update the first time).
        Invoke-Bash "pacman -Syu --noconfirm"
        Invoke-Bash "pacman -Syu --noconfirm"
    }

    Write-Step "Adding devkitPro package repositories..."
    $pacmanConf = Join-Path $Msys2Dir "etc\pacman.conf"
    $confText = Get-Content $pacmanConf -Raw
    if ($confText -notmatch "\[dkp-libs\]") {
        Add-Content $pacmanConf "`n[dkp-libs]`nServer = https://pkg.devkitpro.org/packages`n"
        Add-Content $pacmanConf "`n[dkp-windows]`nServer = https://pkg.devkitpro.org/packages/windows/`$arch/`n"
    }

    # --- Everything from here to the end of this else-block needs internet
    # access to pkg.devkitpro.org: devkitPro only distributes devkitPPC,
    # libogc and the Wii portlibs as pacman packages, so there is no plain
    # file to bundle in vendor/ for this part. ---
    Write-Step "Trusting the devkitPro signing key (needs internet access to pkg.devkitpro.org)..."
    Invoke-Bash "curl -fsSL -o /tmp/devkitpro-keyring.pkg.tar.zst https://pkg.devkitpro.org/devkitpro-keyring.pkg.tar.zst"
    Invoke-Bash "pacman -U --noconfirm /tmp/devkitpro-keyring.pkg.tar.zst"

    Write-Step "Installing devkitPPC, libogc and Wii portlibs (this takes a while)..."
    Invoke-Bash "pacman -Sy --noconfirm"
    Invoke-Bash "pacman -S --needed --noconfirm wii-dev ppc-libpng ppc-freetype ppc-libjpeg-turbo libfat-ogc git"
}

# ---------------------------------------------------------------------------
# Step 2: GRRLIB -- not a pacman package, must be built from source and
# make-installed into portlibs/wii.
# ---------------------------------------------------------------------------
if (Test-Path (Join-Path $DkpDir "portlibs\wii\lib\libgrrlib.a")) {
    Write-Step "GRRLIB already installed -- skipping."
} else {
    Write-Step "Building GRRLIB..."
    $dkpMsys      = To-MsysPath $DkpDir
    $grrlibSrcWin = Join-Path $Msys2Dir "home\grrlib-src"
    $grrlibSrc    = To-MsysPath $grrlibSrcWin

    if (Test-Path $VendoredGrrlib) {
        Write-Host "Using bundled GRRLIB source from vendor/grrlib-src (no git clone needed)."
        Remove-Item $grrlibSrcWin -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $grrlibSrcWin) | Out-Null
        Copy-Item -Path $VendoredGrrlib -Destination $grrlibSrcWin -Recurse -Force
        $cloneCmd = ""
    } else {
        Write-Host "vendor/grrlib-src not found -- cloning GRRLIB from GitHub instead (needs internet access)..."
        $cloneCmd = "rm -rf $grrlibSrc && git clone --depth 1 https://github.com/GRRLIB/GRRLIB.git $grrlibSrc"
    }

    $buildCmd = @"
export DEVKITPRO=$dkpMsys
export DEVKITPPC=`$DEVKITPRO/devkitPPC
export PATH=`$DEVKITPRO/portlibs/wii/bin:`$DEVKITPRO/portlibs/ppc/bin:`$DEVKITPPC/bin:`$PATH
$cloneCmd
cd $grrlibSrc/GRRLIB
make clean all install
"@
    Invoke-Bash $buildCmd

    if (-not (Test-Path (Join-Path $DkpDir "portlibs\wii\lib\libgrrlib.a"))) {
        throw "GRRLIB build finished but libgrrlib.a is still missing -- check the output above."
    }
}

# ---------------------------------------------------------------------------
# Step 3: Wiimms ISO Tools (WIT) -- a plain portable .exe, no build needed.
# Not bundled: Wiimm distributes WIT only from wit.wiimm.de, so this step
# needs internet access to that host.
# ---------------------------------------------------------------------------
if (Test-Path (Join-Path $WitDir "wit.exe")) {
    Write-Step "WIT already present -- skipping."
} else {
    Write-Step "Downloading WIT ($WitVersion) (needs internet access to wit.wiimm.de)..."
    New-Item -ItemType Directory -Force -Path $WitDir | Out-Null
    $witZip = Join-Path $WorkDir "wit.zip"
    try {
        Invoke-WebRequest -Uri $WitUrl -OutFile $witZip
    } catch {
        throw "Couldn't download WIT from $WitUrl -- check https://wit.wiimm.de/download.html for the current version and update `$WitVersion at the top of this script."
    }
    $witExtractTmp = Join-Path $WorkDir "wit-extract"
    Remove-Item $witExtractTmp -Recurse -Force -ErrorAction SilentlyContinue
    Expand-Archive -Path $witZip -DestinationPath $witExtractTmp -Force
    Remove-Item $witZip

    # The zip usually nests everything under a bin/ folder; find wit.exe wherever it landed.
    $witExe = Get-ChildItem -Path $witExtractTmp -Filter "wit.exe" -Recurse | Select-Object -First 1
    if (-not $witExe) { throw "wit.exe not found inside the downloaded WIT archive." }
    Get-ChildItem -Path $witExe.Directory | Copy-Item -Destination $WitDir -Recurse -Force
    Remove-Item $witExtractTmp -Recurse -Force -ErrorAction SilentlyContinue
}

Remove-Item $WorkDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Setup complete. tools/devkitpro and tools/wit are ready." -ForegroundColor Green
Write-Host "You can now run: make iso   (or scripts\build_iso.bat)"

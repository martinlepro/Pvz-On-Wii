param([string]$Zombie = "", [switch]$Validate, [switch]$Force)

Add-Type -AssemblyName System.Drawing

# Find directories dynamically using wildcards to avoid accent issues
$drive = "D:"
$srcCandidates = Get-ChildItem -Path "$drive\" -Directory -Filter "T?l?chargements" -ErrorAction SilentlyContinue
if (-not $srcCandidates) { Write-Error "Cannot find Downloads directory on D:"; exit 1 }
$dlDir = $srcCandidates[0].FullName

$paletteCandidates = Get-ChildItem -Path "$dlDir" -Directory -Filter "PvZ-Wii-color-palette*" -ErrorAction SilentlyContinue
if (-not $paletteCandidates) { Write-Error "Cannot find PvZ-Wii-color-palette directory"; exit 1 }
$srcRoot = Join-Path $paletteCandidates[0].FullName "zombie"

$projectCandidates = Get-ChildItem -Path "$dlDir" -Directory -Filter "pvz_wii_full_project*" -ErrorAction SilentlyContinue
if (-not $projectCandidates) { Write-Error "Cannot find pvz_wii_full_project directory"; exit 1 }
$dstRoot = Join-Path $projectCandidates[0].FullName "assets\zombies"

$configPath = Join-Path $PSScriptRoot "crop_config.json"

Write-Output ("Source: " + $srcRoot)
Write-Output ("Dest:   " + $dstRoot)

if (-not (Test-Path $configPath)) { Write-Error "Config not found: $configPath"; exit 1 }
$config = Get-Content $configPath -Raw -Encoding UTF8 | ConvertFrom-Json

function Convert-WhiteToTransparent($bmp) {
    $w = $bmp.Width; $h = $bmp.Height
    $result = New-Object System.Drawing.Bitmap $w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    for ($y = 0; $y -lt $h; $y++) {
        for ($x = 0; $x -lt $w; $x++) {
            $c = $bmp.GetPixel($x, $y)
            if ($c.R -ge 248 -and $c.G -ge 248 -and $c.B -ge 248) {
                $result.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, $c.R, $c.G, $c.B))
            } else {
                $result.SetPixel($x, $y, $c)
            }
        }
    }
    return $result
}

function Find-MainBlob($bmp) {
    $w = $bmp.Width; $h = $bmp.Height; $step = 8
    $visited = New-Object 'bool[,]' ($w/$step+1), ($h/$step+1)
    $largest = $null; $largestArea = 0
    for ($sy = 0; $sy -lt $h; $sy += $step) {
        for ($sx = 0; $sx -lt $w; $sx += $step) {
            $vi = [int]($sx/$step); $vj = [int]($sy/$step)
            if ($visited[$vi,$vj]) { continue }
            $c = $bmp.GetPixel($sx, $sy)
            if ($c.R -ge 240 -and $c.G -ge 240 -and $c.B -ge 240) { continue }
            $minX = $sx; $maxX = $sx; $minY = $sy; $maxY = $sy
            $stack = New-Object System.Collections.Generic.Stack[System.Drawing.Point]
            $stack.Push((New-Object System.Drawing.Point $sx, $sy))
            while ($stack.Count -gt 0) {
                $p = $stack.Pop()
                $pi = [int]($p.X/$step); $pj = [int]($p.Y/$step)
                if ($pi -lt 0 -or $pi -ge $visited.GetLength(0) -or $pj -lt 0 -or $pj -ge $visited.GetLength(1)) { continue }
                if ($visited[$pi,$pj]) { continue }
                $visited[$pi,$pj] = $true
                $pc = $bmp.GetPixel($p.X, $p.Y)
                if ($pc.R -ge 240 -and $pc.G -ge 240 -and $pc.B -ge 240) { continue }
                if ($p.X -lt $minX) { $minX = $p.X }
                if ($p.X -gt $maxX) { $maxX = $p.X }
                if ($p.Y -lt $minY) { $minY = $p.Y }
                if ($p.Y -gt $maxY) { $maxY = $p.Y }
                $stack.Push((New-Object System.Drawing.Point ($p.X-$step),$p.Y))
                $stack.Push((New-Object System.Drawing.Point ($p.X+$step),$p.Y))
                $stack.Push((New-Object System.Drawing.Point $p.X,($p.Y-$step)))
                $stack.Push((New-Object System.Drawing.Point $p.X,($p.Y+$step)))
            }
            $area = ($maxX-$minX+1)*($maxY-$minY+1)
            if ($area -gt $largestArea) { $largestArea = $area; $largest = @{x=$minX; y=$minY; w=$maxX-$minX+1; h=$maxY-$minY+1} }
        }
    }
    return $largest
}

foreach ($entry in $config.PSObject.Properties) {
    $slug = $entry.Name
    if ($Zombie -ne "" -and $slug -ne $Zombie) { continue }
    $info = $entry.Value
    $srcFile = Join-Path $srcRoot $info.src
    if (-not (Test-Path $srcFile)) { Write-Warning ("Source not found: " + $srcFile); continue }
    Write-Output ("Processing: " + $slug)
    $zombieDir = Join-Path $dstRoot $slug
    $partsDir = Join-Path $zombieDir "parts"
    if (-not (Test-Path $partsDir)) { New-Item -ItemType Directory -Path $partsDir -Force | Out-Null }
    $srcBmp = [System.Drawing.Bitmap]::FromFile($srcFile)
    $full = $info.full
    if (-not $full.x -or $full.x -eq 0) {
        $blob = Find-MainBlob $srcBmp
        if ($blob) { $full.x = $blob.x; $full.y = $blob.y; $full.w = $blob.w; $full.h = $blob.h }
    }
    foreach ($partEntry in $info.parts.PSObject.Properties) {
        $pn = $partEntry.Name; $pi = $partEntry.Value
        $targetW = $pi.tw; $targetH = $pi.th; $cx = $pi.x; $cy = $pi.y
        if (-not $cx) { $cx = $full.x + $full.w/4; $cy = $full.y + $full.h/4 }
        $outPath = Join-Path $partsDir $pn
        if ((Test-Path $outPath) -and -not $Force) { Write-Output ("  " + $pn + " exists, skip"); continue }
        $cw = $targetW; $ch = $targetH
        if ($cx+$cw -gt $srcBmp.Width) { $cw = $srcBmp.Width-$cx }
        if ($cy+$ch -gt $srcBmp.Height) { $ch = $srcBmp.Height-$cy }
        if ($cw -le 0 -or $ch -le 0) { Write-Warning ("Invalid crop for " + $pn); continue }
        $crop = New-Object System.Drawing.Rectangle($cx, $cy, $cw, $ch)
        $cropped = $srcBmp.Clone($crop, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $resized = New-Object System.Drawing.Bitmap($targetW, $targetH)
        $g = [System.Drawing.Graphics]::FromImage($resized)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($cropped, 0, 0, $targetW, $targetH); $g.Dispose(); $cropped.Dispose()
        $t = Convert-WhiteToTransparent $resized; $resized.Dispose()
        $t.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png); $t.Dispose()
        Write-Output ("  Saved: " + $pn + " (" + $targetW + "x" + $targetH + ")")
    }
    $fullOut = Join-Path $zombieDir "full.png"
    if ((Test-Path $fullOut) -and -not $Force) { Write-Output "  full.png exists, skip" }
    elseif ($full.w -gt 0 -and $full.h -gt 0) {
        $fr = New-Object System.Drawing.Rectangle($full.x, $full.y, $full.w, $full.h)
        $fc = $srcBmp.Clone($fr, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $fr2 = New-Object System.Drawing.Bitmap($full.w, $full.h)
        $g = [System.Drawing.Graphics]::FromImage($fr2)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($fc, 0, 0, $full.w, $full.h); $g.Dispose(); $fc.Dispose()
        $t = Convert-WhiteToTransparent $fr2; $fr2.Dispose()
        $t.Save($fullOut, [System.Drawing.Imaging.ImageFormat]::Png); $t.Dispose()
        Write-Output ("  Saved full.png (" + $full.w + "x" + $full.h + ")")
    }
    $srcBmp.Dispose()
}

if ($Validate) {
    Write-Output "`n=== VALIDATION ==="
    $issues = @()
    Get-ChildItem -Path $dstRoot -Directory | ForEach-Object {
        $partsDir = Join-Path $_.FullName "parts"
        if (Test-Path $partsDir) {
            Get-ChildItem -Path $partsDir -Filter "*.png" | ForEach-Object {
                $bmp = [System.Drawing.Bitmap]::FromFile($_.FullName)
                $nonWhite = 0; $total = $bmp.Width * $bmp.Height
                for ($y = 0; $y -lt $bmp.Height; $y += 4) {
                    for ($x = 0; $x -lt $bmp.Width; $x += 4) {
                        $c = $bmp.GetPixel($x, $y)
                        if ($c.A -gt 0 -and ($c.R -lt 248 -or $c.G -lt 248 -or $c.B -lt 248)) { $nonWhite++ }
                    }
                }
                $pct = [int]($nonWhite*100/$total)
                if ($pct -lt 5) { $issues += "LOW CONTENT: " + $_.FullName + " (" + $pct + "%)" }
                Write-Output ("  " + $_.Name + ": " + $bmp.Width + "x" + $bmp.Height + " content=" + $pct + "%")
                $bmp.Dispose()
            }
        }
    }
    if ($issues.Count -gt 0) { Write-Output "`nISSUES:"; $issues | ForEach-Object { Write-Output "  $_" } }
    else { Write-Output "`nAll images look good!" }
}

Write-Output "Done."

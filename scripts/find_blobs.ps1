Add-Type -AssemblyName System.Drawing

$path = "D:\Téléchargements\PvZ-Wii-color-palette-main\zombie\basic.png"
$bmp = [System.Drawing.Bitmap]::FromFile($path)
$w = $bmp.Width
$h = $bmp.Height

$step = 4
$visited = New-Object 'bool[,]' ($w/$step+1), ($h/$step+1)
$blobs = @()

for ($sy = 0; $sy -lt $h; $sy += $step) {
    for ($sx = 0; $sx -lt $w; $sx += $step) {
        $vi = [int]($sx/$step)
        $vj = [int]($sy/$step)
        if ($visited[$vi,$vj]) { continue }
        $c = $bmp.GetPixel($sx, $sy)
        if ($c.R -lt 240 -or $c.G -lt 240 -or $c.B -lt 240) {
            $minX = $sx; $maxX = $sx
            $minY = $sy; $maxY = $sy
            $stack = New-Object System.Collections.Generic.Stack[System.Drawing.Point]
            $stack.Push((New-Object System.Drawing.Point $sx, $sy))
            
            while ($stack.Count -gt 0) {
                $p = $stack.Pop()
                $pi = [int]($p.X/$step)
                $pj = [int]($p.Y/$step)
                if ($pi -lt 0 -or $pi -ge $visited.GetLength(0) -or $pj -lt 0 -or $pj -ge $visited.GetLength(1)) { continue }
                if ($visited[$pi,$pj]) { continue }
                $visited[$pi,$pj] = $true
                $pc = $bmp.GetPixel($p.X, $p.Y)
                if ($pc.R -ge 240 -and $pc.G -ge 240 -and $pc.B -ge 240) { continue }
                if ($p.X -lt $minX) { $minX = $p.X }
                if ($p.X -gt $maxX) { $maxX = $p.X }
                if ($p.Y -lt $minY) { $minY = $p.Y }
                if ($p.Y -gt $maxY) { $maxY = $p.Y }
                $stack.Push((New-Object System.Drawing.Point ($p.X-$step), $p.Y))
                $stack.Push((New-Object System.Drawing.Point ($p.X+$step), $p.Y))
                $stack.Push((New-Object System.Drawing.Point $p.X, ($p.Y-$step)))
                $stack.Push((New-Object System.Drawing.Point $p.X, ($p.Y+$step)))
            }
            
            $blobs += @(New-Object PSObject -Property @{
                x=$minX; y=$minY
                w=$maxX-$minX+1; h=$maxY-$minY+1
            })
        }
    }
}

$sorted = $blobs | Sort-Object y, x
$i = 0
$sorted | ForEach-Object {
    if ($_.w -gt 30 -and $_.h -gt 30) {
        Write-Output ("Blob $i : x=" + $_.x + " y=" + $_.y + " w=" + $_.w + " h=" + $_.h)
        $i++
    }
}

$bmp.Dispose()
Write-Output ("`nTotal blobs found: " + $i)

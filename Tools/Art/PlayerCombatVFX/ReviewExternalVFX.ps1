param([Parameter(Mandatory=$true)][string]$Run, [int]$Build = 0)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sheet = New-Object System.Drawing.Bitmap(1288,1950)
$graphics = [System.Drawing.Graphics]::FromImage($sheet)
$font = New-Object System.Drawing.Font('Segoe UI',11)
try {
    $graphics.Clear([System.Drawing.Color]::FromArgb(23,27,35))
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $index = 0
    foreach ($skill in @(100,101,102,111,112,110,113,114)) {
        $p1 = [int]($skill -in @(110,113,114))
        $phases = switch ($skill) { 110 {5} 111 {4} 112 {6} 113 {5} default {1} }
        for ($phase = 0; $phase -lt $phases; $phase++) {
            $suffix = if ($phase -eq 0) { '' } else { "_Phase_$phase" }
            $miss = if ($Build -eq 0) { '' } else { '_Miss' }
            $path = Join-Path $Run "render_b${Build}_p${p1}/User/Saved/QA/HackSlashP0/Skill_${skill}${miss}${suffix}.png"
            $frame = [System.Drawing.Image]::FromFile($path)
            try {
                $x = ($index % 4)*322
                $y = [math]::Floor($index/4)*325
                $graphics.DrawString("Skill $skill / Contact $($phase+1)",$font,[System.Drawing.Brushes]::White,[single]($x+8),[single]($y+3))
                $target = New-Object System.Drawing.Rectangle($x,($y+24),322,301)
                $source = New-Object System.Drawing.Rectangle(420,220,460,430)
                $graphics.DrawImage($frame,$target,$source,[System.Drawing.GraphicsUnit]::Pixel)
            } finally { $frame.Dispose() }
            $index++
        }
    }
    $output = Join-Path $Run "ExternalVFX_Build${Build}.png"
    $sheet.Save($output,[System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $output
} finally { $font.Dispose(); $graphics.Dispose(); $sheet.Dispose() }

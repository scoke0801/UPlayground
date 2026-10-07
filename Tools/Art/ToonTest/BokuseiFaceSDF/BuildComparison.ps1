param(
    [Parameter(Mandatory=$true)][string]$RenderDirectory,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sheet = [System.Drawing.Bitmap]::new(1560,1210)
$graphics = [System.Drawing.Graphics]::FromImage($sheet)
$graphics.Clear([System.Drawing.Color]::FromArgb(15,20,29))
$font = [System.Drawing.Font]::new('Malgun Gothic',22)
$smallFont = [System.Drawing.Font]::new('Malgun Gothic',17)
$white = [System.Drawing.Brushes]::White
$graphics.DrawString('Bokusei · 동일 모델 / UV / 포즈 / 조명',$font,$white,20,12)
$angles = @('-060','+000','+060')
foreach ($row in 0..1) {
    $stage = if ($row -eq 0) { 5 } else { 7 }
    $label = if ($row -eq 0) { '기존 얼굴 명암' } else { '모델 기반 얼굴 SDF 적용' }
    $top = 64 + $row*570
    $graphics.DrawString($label,$font,$white,20,$top)
    foreach ($col in 0..2) {
        $path = Join-Path $RenderDirectory ("Stage${stage}Light$($angles[$col]).png")
        $source = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $path))
        try {
            if ($source.Width -ne 1600 -or $source.Height -ne 900) { throw 'Expected 1600x900 source captures' }
            $destination = [System.Drawing.Rectangle]::new(($col*520),($top+42),520,500)
            $crop = [System.Drawing.Rectangle]::new(550,170,520,500)
            $graphics.DrawImage($source,$destination,$crop,[System.Drawing.GraphicsUnit]::Pixel)
            $angle = if ($col -eq 0) { '좌측 조명 -60°' } elseif ($col -eq 1) { '정면 조명 0°' } else { '우측 조명 +60°' }
            $graphics.DrawString($angle,$smallFont,$white,$col*520+20,$top+509)
        } finally { $source.Dispose() }
    }
}
try { $sheet.Save([System.IO.Path]::GetFullPath($Output),[System.Drawing.Imaging.ImageFormat]::Png) }
finally { $graphics.Dispose();$font.Dispose();$smallFont.Dispose();$sheet.Dispose() }
Write-Output $Output

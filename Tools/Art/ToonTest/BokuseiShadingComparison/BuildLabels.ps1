$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Write-PGLabel {
    param([string]$Name, [string]$Title, [string]$Subtitle, [int]$Width, [int]$Height, [int]$TitleSize)
    $pgBitmap = New-Object System.Drawing.Bitmap($Width, $Height)
    $pgGraphics = [System.Drawing.Graphics]::FromImage($pgBitmap)
    $pgGraphics.Clear([System.Drawing.Color]::Transparent)
    $pgGraphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $pgTitleFont = New-Object System.Drawing.Font('Malgun Gothic', $TitleSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $pgSubtitleFont = New-Object System.Drawing.Font('Malgun Gothic', 27, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $pgTitleBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 235, 240, 248))
    $pgSubtitleBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 158, 176, 198))
    $pgFormat = New-Object System.Drawing.StringFormat
    $pgFormat.Alignment = [System.Drawing.StringAlignment]::Center
    $pgGraphics.DrawString($Title, $pgTitleFont, $pgTitleBrush, [System.Drawing.RectangleF]::new(0, 3, $Width, $TitleSize + 18), $pgFormat)
    $pgGraphics.DrawString($Subtitle, $pgSubtitleFont, $pgSubtitleBrush, [System.Drawing.RectangleF]::new(0, $TitleSize + 24, $Width, 45), $pgFormat)
    $pgBitmap.Save((Join-Path $PSScriptRoot ($Name + '.png')), [System.Drawing.Imaging.ImageFormat]::Png)
    $pgFormat.Dispose()
    $pgTitleBrush.Dispose()
    $pgSubtitleBrush.Dispose()
    $pgTitleFont.Dispose()
    $pgSubtitleFont.Dispose()
    $pgGraphics.Dispose()
    $pgBitmap.Dispose()
}

Write-PGLabel 'Title' 'bOKUSEI 단계별 셰이딩 비교' '같은 메시 · 텍스처 · 포즈 / 왼쪽부터 순서대로 적용' 1600 150 58
Write-PGLabel 'Lit' '1 · 일반 조명' '부드러운 명암 · 기준' 640 128 43
Write-PGLabel 'Cel' '2 · 셀 명암' '공통 3단 명암' 640 128 43
Write-PGLabel 'Parts' '3 · 부위별 명암' '피부 · 얼굴 · 헤어 튜닝' 640 128 43
Write-PGLabel 'Rim' '4 · 림·하이라이트' '윤곽 빛 · 재질별 반사' 640 128 43
Write-PGLabel 'Toon' '5 · 외곽선 · 기존 툰' '현재 bOKUSEI 표현' 640 128 40
Write-PGLabel 'Shadow' '6 · 월드 그림자' '툰 + 월드 조명 · 헤어 그림자' 640 128 43
Write-PGLabel 'FaceSDF' '7 · 얼굴 SDF' '모델 기반 얼굴 명암 · 기존 툰' 640 128 43
Write-PGLabel 'FaceSDFWorld' '8 · 얼굴 SDF · 그림자' '얼굴 명암 + 월드 · 헤어 그림자' 640 128 38
Write-PGLabel 'Arin' '아린' '8단계 셰이딩 비교 · 공통 조명' 640 128 43
Write-PGLabel 'Hwarin' '화련' '8단계 셰이딩 비교 · 공통 조명' 640 128 43
Write-PGLabel 'LianLian' 'Lianlian' '8단계 셰이딩 비교 · 공통 조명' 640 128 43

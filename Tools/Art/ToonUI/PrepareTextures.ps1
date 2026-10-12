$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$source = [System.Drawing.Image]::FromFile((Join-Path $PSScriptRoot 'T_PGToonFrame_Source.png'))
try {
 foreach ($size in @(256,128)) {
  $name = if ($size -eq 256) { 'T_PGToonPanel' } else { 'T_PGToonPlate' }
  $bitmap = New-Object System.Drawing.Bitmap($size,$size,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  try {
   $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
   $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
   $graphics.DrawImage($source,0,0,$size,$size)
   $bitmap.Save((Join-Path $PSScriptRoot ($name+'.png')),[System.Drawing.Imaging.ImageFormat]::Png)
  } finally { $graphics.Dispose(); $bitmap.Dispose() }
 }
} finally { $source.Dispose() }

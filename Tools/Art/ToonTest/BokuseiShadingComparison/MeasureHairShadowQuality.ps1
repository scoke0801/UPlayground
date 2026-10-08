param([Parameter(Mandatory=$true)][string]$CaptureDirectory)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
public static class PGHairShadowPixels {
    static bool Skin(Color c) {
        return c.R >= 100 && c.G >= 80 && c.B >= 70 &&
            c.R-c.G >= 0 && c.R-c.G <= 45 && Math.Abs(c.G-c.B) <= 30;
    }
    static double Luma(Color c) { return .2126*c.R + .7152*c.G + .0722*c.B; }
    public static double[] Measure(string beforePath, string afterPath, string offPath) {
        using (var before = new Bitmap(beforePath))
        using (var after = new Bitmap(afterPath))
        using (var off = new Bitmap(offPath)) {
            if (before.Size != off.Size || after.Size != off.Size)
                throw new Exception("Capture sizes differ");
            int count=0, changed=0, gradientCount=0;
            double darkness=0, oldVariation=0, newVariation=0, oldEnergy=0, newEnergy=0;
            for (int y=(int)(off.Height*.44); y<(int)(off.Height*.67); y++)
            for (int x=(int)(off.Width*.43); x<(int)(off.Width*.61); x++) {
                Color reference=off.GetPixel(x,y);
                if (!Skin(reference)) continue;
                double oldShadow=Luma(reference)-Luma(before.GetPixel(x,y));
                double newShadow=Luma(reference)-Luma(after.GetPixel(x,y));
                darkness+=newShadow; count++;
                if (newShadow>6) changed++;
                // Exclude silhouettes, eyes, hair and skin/texture boundaries.
                // Measure adjacent differences in the isolated shadow field.
                foreach (Point step in new[] { new Point(1,0), new Point(0,1) }) {
                    Color next=off.GetPixel(x+step.X,y+step.Y);
                    if (!Skin(next) || Math.Abs(Luma(reference)-Luma(next))>3) continue;
                    double oldNext=Luma(next)-Luma(before.GetPixel(x+step.X,y+step.Y));
                    double newNext=Luma(next)-Luma(after.GetPixel(x+step.X,y+step.Y));
                    oldVariation+=Math.Abs(oldShadow-oldNext);
                    newVariation+=Math.Abs(newShadow-newNext);
                    oldEnergy+=(oldShadow-oldNext)*(oldShadow-oldNext);
                    newEnergy+=(newShadow-newNext)*(newShadow-newNext);
                    gradientCount++;
                }
            }
            if (count<800 || gradientCount<800) throw new Exception("Insufficient exposed skin samples");
            return new[] { (double)count, darkness/count, (double)changed/count,
                oldVariation/gradientCount, newVariation/gradientCount,
                newVariation/Math.Max(oldVariation,.000001),
                Math.Sqrt(oldEnergy/gradientCount), Math.Sqrt(newEnergy/gradientCount),
                Math.Sqrt(newEnergy/Math.Max(oldEnergy,.000001)) };
        }
    }
}
'@
$pgReport = @{ status='RUNNING'; capture=$CaptureDirectory; pairs=@() }
try {
    $pgCapture = Get-Content -LiteralPath (Join-Path $CaptureDirectory 'quality.json') -Raw | ConvertFrom-Json
    if ($pgCapture.status -ne 'PASS') { throw 'Candidate capture did not pass' }
    foreach ($pgStage in @(6,8)) {
        foreach ($pgView in @('Front','Quarter')) {
            $pgValues = [PGHairShadowPixels]::Measure(
                (Join-Path $CaptureDirectory "BeforeStage${pgStage}${pgView}.png"),
                (Join-Path $CaptureDirectory "CandidateStage${pgStage}${pgView}.png"),
                (Join-Path $CaptureDirectory "CandidateStage${pgStage}${pgView}Off.png"))
            $pgPair = @{ stage=$pgStage; view=$pgView; samples=$pgValues[0]; mean_darkening=$pgValues[1];
                changed_fraction=$pgValues[2]; before_shadow_variation=$pgValues[3];
                after_shadow_variation=$pgValues[4]; variation_ratio=$pgValues[5];
                before_shadow_gradient_rms=$pgValues[6]; after_shadow_gradient_rms=$pgValues[7];
                gradient_rms_ratio=$pgValues[8] }
            $pgReport.pairs += $pgPair
            if ($pgPair.mean_darkening -lt .5 -or $pgPair.changed_fraction -lt .015) {
                throw "Hair shadow lost on exposed skin: stage $pgStage $pgView"
            }
            # Squared gradient energy detects sharp edges. Total variation of a
            # monotonic edge stays constant even when that edge becomes softer.
            if ($pgPair.gradient_rms_ratio -gt .95) {
                throw "Shadow edges not sufficiently softened: stage $pgStage $pgView"
            }
        }
    }
    $pgSheet = [System.Drawing.Bitmap]::new(1120,660)
    $pgGraphics = [System.Drawing.Graphics]::FromImage($pgSheet)
    $pgFont = [System.Drawing.Font]::new('Malgun Gothic',17)
    try {
        $pgGraphics.Clear([System.Drawing.Color]::FromArgb(12,17,25))
        $pgGraphics.DrawString('기존 머리카락 그림자', $pgFont, [System.Drawing.Brushes]::White, 25, 15)
        $pgGraphics.DrawString('개선한 머리카락 그림자', $pgFont, [System.Drawing.Brushes]::White, 585, 15)
        foreach ($pgColumn in @(0,1)) {
            $pgName = if ($pgColumn -eq 0) { 'BeforeStage8Front.png' } else { 'CandidateStage8Front.png' }
            $pgImage = [System.Drawing.Bitmap]::new((Join-Path $CaptureDirectory $pgName))
            try {
                $pgGraphics.DrawImage($pgImage, [System.Drawing.Rectangle]::new($pgColumn*560+10,60,540,580),
                    [System.Drawing.Rectangle]::new(630,260,430,462), [System.Drawing.GraphicsUnit]::Pixel)
            }
            finally { $pgImage.Dispose() }
        }
        $pgSheet.Save((Join-Path $CaptureDirectory 'Comparison.png'),[System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $pgGraphics.Dispose(); $pgFont.Dispose(); $pgSheet.Dispose() }
    $pgReport.status='PASS'
}
catch { $pgReport.status='FAIL'; $pgReport.error=$_.Exception.Message; throw }
finally {
    $pgPayload=$pgReport | ConvertTo-Json -Depth 5
    $pgPayload | Set-Content -LiteralPath (Join-Path $CaptureDirectory 'shadow_quality_pixels.json') -Encoding UTF8
    Write-Output $pgPayload
}

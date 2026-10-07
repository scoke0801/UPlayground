param([Parameter(Mandatory=$true)][string]$PreviewDirectory)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Measure-PGShadowPair {
    param([string]$On, [string]$Off, [switch]$SkinOnly)
    $pgOnImage = [System.Drawing.Bitmap]::new($On)
    $pgOffImage = [System.Drawing.Bitmap]::new($Off)
    try {
        if ($pgOnImage.Size -ne $pgOffImage.Size) { throw 'Shadow comparison image sizes differ' }
        $pgAbs = 0.0
        $pgDarkening = 0.0
        $pgChanged = 0
        $pgSamples = 0
        # Central face/hair region; excludes the floor, signs and changing HUD.
        $pgMinY = .36; $pgMaxY = .76; $pgMinX = .37; $pgMaxX = .63
        if ($SkinOnly) { $pgMinY = .44; $pgMaxY = .67; $pgMinX = .43; $pgMaxX = .61 }
        for ($pgY = [int]($pgOnImage.Height * $pgMinY); $pgY -lt [int]($pgOnImage.Height * $pgMaxY); $pgY += 2) {
            for ($pgX = [int]($pgOnImage.Width * $pgMinX); $pgX -lt [int]($pgOnImage.Width * $pgMaxX); $pgX += 2) {
                $pgA = $pgOnImage.GetPixel($pgX, $pgY)
                $pgB = $pgOffImage.GetPixel($pgX, $pgY)
                # Select exposed skin from the no-hair-shadow frame, excluding
                # dark hair, red eyes and clothing so self-shadow can't pass this check.
                if ($SkinOnly -and ($pgB.R -lt 100 -or $pgB.G -lt 80 -or $pgB.B -lt 70 -or
                    $pgB.R - $pgB.G -lt 0 -or $pgB.R - $pgB.G -gt 45 -or [Math]::Abs($pgB.G - $pgB.B) -gt 30)) { continue }
                $pgDelta = ([Math]::Abs([int]$pgA.R - [int]$pgB.R) + [Math]::Abs([int]$pgA.G - [int]$pgB.G) + [Math]::Abs([int]$pgA.B - [int]$pgB.B)) / 3.0
                $pgAbs += $pgDelta
                $pgDarkening += (.2126 * ($pgB.R - $pgA.R) + .7152 * ($pgB.G - $pgA.G) + .0722 * ($pgB.B - $pgA.B))
                if ($pgDelta -gt 6) { $pgChanged++ }
                $pgSamples++
            }
        }
        if ($pgSamples -lt 200) { throw 'Insufficient face samples' }
        return @{ mean_rgb_delta = $pgAbs / $pgSamples; mean_darkening = $pgDarkening / $pgSamples; changed_fraction = $pgChanged / [double]$pgSamples; samples = $pgSamples }
    }
    finally { $pgOnImage.Dispose(); $pgOffImage.Dispose() }
}

$pgReport = @{ status = 'RUNNING'; preview = $PreviewDirectory }
try {
    $pgBaseline = Measure-PGShadowPair (Join-Path $PreviewDirectory 'Stage5ShadowOn.png') (Join-Path $PreviewDirectory 'Stage5ShadowOff.png')
    $pgWorld = Measure-PGShadowPair (Join-Path $PreviewDirectory 'Stage6ShadowOn.png') (Join-Path $PreviewDirectory 'Stage6ShadowOff.png')
    $pgPlay = Measure-PGShadowPair (Join-Path $PreviewDirectory 'PlayableShadowOn.png') (Join-Path $PreviewDirectory 'PlayableShadowOff.png')
    $pgReport.baseline_unlit = $pgBaseline
    $pgReport.world_lit = $pgWorld
    $pgReport.play_input = $pgPlay
    $pgHair = Measure-PGShadowPair (Join-Path $PreviewDirectory 'Stage6Face.png') (Join-Path $PreviewDirectory 'Stage6HairShadowOff.png') -SkinOnly
    $pgHairQuarter = Measure-PGShadowPair (Join-Path $PreviewDirectory 'Stage6HairQuarterOn.png') (Join-Path $PreviewDirectory 'Stage6HairQuarterOff.png') -SkinOnly
    $pgHairPlay = Measure-PGShadowPair (Join-Path $PreviewDirectory 'PlayableHairShadowOn.png') (Join-Path $PreviewDirectory 'PlayableHairShadowOff.png') -SkinOnly
    $pgReport.hair_face = $pgHair
    $pgReport.hair_quarter = $pgHairQuarter
    $pgReport.hair_play_input = $pgHairPlay
    if ($pgBaseline.mean_rgb_delta -gt 1) { throw 'Unlit baseline unexpectedly responds to the caster' }
    if ($pgWorld.mean_darkening -lt 2 -or $pgWorld.changed_fraction -lt .03) { throw 'No visible world-lit shadow reception' }
    if ($pgPlay.mean_darkening -lt 2 -or $pgPlay.changed_fraction -lt .03) { throw 'H input did not produce visible shadow reception' }
    foreach ($pgHairPair in @($pgHair, $pgHairQuarter, $pgHairPlay)) {
        if ($pgHairPair.mean_darkening -lt .5 -or $pgHairPair.changed_fraction -lt .015) { throw 'No visible hair shadow on exposed face skin' }
    }
    $pgReport.status = 'PASS'
}
catch { $pgReport.status = 'FAIL'; $pgReport.error = $_.Exception.Message; throw }
finally {
    $pgPayload = $pgReport | ConvertTo-Json -Depth 5
    $pgPayload | Set-Content -LiteralPath (Join-Path $PreviewDirectory 'shadow_pixels.json') -Encoding UTF8
    Write-Output $pgPayload
}

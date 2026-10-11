[CmdletBinding()]
param(
    [string]$Session,
    [ValidateSet('1280x720', '1920x1080', '2560x1080')]
    [string]$Size = '1920x1080',
    [string]$PackageReport,
    [switch]$PrepareOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectData = Get-Content -LiteralPath (Join-Path $projectRoot 'UPlayground.uproject') -Raw | ConvertFrom-Json
$python = Join-Path $env:ProgramFiles ('Epic Games/UE_' + $projectData.EngineAssociation + '/Engine/Binaries/ThirdParty/Python3/Win64/python.exe')
$launchArgs = @((Join-Path $PSScriptRoot 'Validation/PlayInventoryAcceptance.py'), '--size', $Size)
if ($Session) { $launchArgs += @('--session', $Session) }
if ($PackageReport) { $launchArgs += @('--package-report', $PackageReport) }
if ($PrepareOnly) { $launchArgs += '--prepare-only' }
& $python @launchArgs
exit $LASTEXITCODE

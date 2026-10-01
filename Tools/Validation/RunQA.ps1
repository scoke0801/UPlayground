[CmdletBinding()]
param(
    [ValidateSet('quick', 'full')][string]$Suite = 'full',
    [string]$EngineRoot,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if (-not $EngineRoot) {
    $projectData = Get-Content -LiteralPath (Join-Path $projectRoot 'UPlayground.uproject') -Raw | ConvertFrom-Json
    $EngineRoot = Join-Path $env:ProgramFiles ('Epic Games/UE_' + $projectData.EngineAssociation)
}
$EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$pythonExe = Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
$runnerArgs = @((Join-Path $PSScriptRoot 'RunQA.py'), '--engine-root', $EngineRoot, '--suite', $Suite)
if ($SkipBuild) { $runnerArgs += '--skip-build' }
& $pythonExe @runnerArgs
exit $LASTEXITCODE

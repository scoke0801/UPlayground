[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'UPlayground.uproject'
$projectData = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$editorExe = Join-Path $env:ProgramFiles ('Epic Games/UE_' + $projectData.EngineAssociation + '/Engine/Binaries/Win64/UnrealEditor.exe')
$bossFile = Join-Path $projectRoot 'Content/DataCenter/DarkKnightBoss/BP_15601.uasset'
if (-not (Test-Path -LiteralPath $bossFile)) { throw 'Dark Knight 보스 데이터를 먼저 생성해야 합니다.' }
$scriptFile = Join-Path $projectRoot 'Tools/Validation/PlayDarkKnightBoss.py'
$launchArgs = @($projectFile, '-culture=ko', '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin', '-PGTestProfile=DarkKnightPlay', ('-ExecutePythonScript=' + $scriptFile))
& $editorExe @launchArgs
exit $LASTEXITCODE

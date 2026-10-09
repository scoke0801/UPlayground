[CmdletBinding()]
param([switch]$Editor)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'UPlayground.uproject'
$projectData = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$editorExe = Join-Path $env:ProgramFiles ('Epic Games/UE_' + $projectData.EngineAssociation + '/Engine/Binaries/Win64/UnrealEditor.exe')
if (-not (Test-Path -LiteralPath $editorExe)) { throw '에디터 실행 파일을 찾지 못했습니다.' }
$mapFile = Join-Path $projectRoot 'Content/Maps/L_PG_ForestRuins.umap'
if (-not (Test-Path -LiteralPath $mapFile)) { throw '숲속 폐허 맵을 먼저 생성해야 합니다.' }
$launchArgs = @($projectFile, '/Game/Maps/L_PG_ForestRuins', '-culture=ko', '-DisablePlugins=RiderLink')
if (-not $Editor) { $launchArgs += @('-game', '-windowed', '-ResX=1600', '-ResY=900') }
& $editorExe @launchArgs
exit $LASTEXITCODE

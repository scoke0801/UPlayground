[CmdletBinding()]
param([ValidateSet('Development', 'DebugGame')][string]$Configuration = 'Development')

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'UPlayground.uproject'
$projectData = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$executable = if ($Configuration -eq 'DebugGame') { 'UnrealEditor-Win64-DebugGame.exe' } else { 'UnrealEditor.exe' }
$editorExe = Join-Path $env:ProgramFiles ('Epic Games/UE_' + $projectData.EngineAssociation + '/Engine/Binaries/Win64/' + $executable)
if (-not (Test-Path -LiteralPath $editorExe)) { throw '에디터 실행 파일을 찾지 못했습니다.' }
& $editorExe $projectFile '/Game/Maps/RogueArena' -game -windowed -ResX=1600 -ResY=900 -culture=ko -DisablePlugins=RiderLink
exit $LASTEXITCODE

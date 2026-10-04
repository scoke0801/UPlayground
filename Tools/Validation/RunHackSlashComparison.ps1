# Forward CLI arguments to the engine's bundled Python. No PATH setup required.
$pgRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$pgVersion = (Get-Content -LiteralPath (Join-Path $pgRoot 'UPlayground.uproject') -Raw | ConvertFrom-Json).EngineAssociation
$pgPython = Join-Path $env:ProgramFiles "Epic Games/UE_$pgVersion/Engine/Binaries/ThirdParty/Python3/Win64/python.exe"
& $pgPython (Join-Path $PSScriptRoot 'RunHackSlashComparison.py') @args
exit $LASTEXITCODE

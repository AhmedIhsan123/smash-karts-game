# Builds the editor target from the command line (editor must be closed).
# Usage: powershell -ExecutionPolicy Bypass -File Tools\Build.ps1 [-GenerateProjectFiles]
param(
    [switch]$GenerateProjectFiles,
    [string]$EngineDir = "C:\Program Files\Epic Games\UE_5.8",
    [string]$Target = "SmashKartsEditor",
    [string]$Configuration = "Development"
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Repo "SmashKarts.uproject"
$Batch = Join-Path $EngineDir "Engine\Build\BatchFiles"

if ($GenerateProjectFiles) {
    & (Join-Path $Batch "GenerateProjectFiles.bat") "-project=$Project" -game
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& (Join-Path $Batch "Build.bat") $Target Win64 $Configuration "-Project=$Project" -WaitMutex -FromMsBuild
exit $LASTEXITCODE

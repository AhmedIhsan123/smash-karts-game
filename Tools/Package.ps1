# Packages the game for friends: Build\KartArena_dev_v<version>.zip (Development) or Build\KartArena_v<version>.zip (Shipping).
# Safe while Unreal Editor is open: it mirrors Config, Content and Source into a work folder and builds, cooks and
# packages that copy, so the open editor's binaries and assets are never touched. The work folder is kept between
# runs (incremental builds). Save your assets in the editor first: the copy takes what's on disk.
# Config/Windows/WindowsEngine.ini (EOS credentials, online play) goes into the build when it exists.
# Usage: powershell -ExecutionPolicy Bypass -File Tools\Package.ps1 [-Configuration Shipping]
param(
    [ValidateSet("Development", "Shipping")][string]$Configuration = "Development",
    [string]$EngineDir = "C:\Program Files\Epic Games\UE_5.8",
    [string]$WorkDir = (Join-Path $env:LOCALAPPDATA "KartArenaPackage")
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent $PSScriptRoot
$Copy = Join-Path $WorkDir "Project"
$Archive = Join-Path $WorkDir "Archive"
$Batch = Join-Path $EngineDir "Engine\Build\BatchFiles"

$VersionLine = Select-String -Path (Join-Path $Repo "Config\DefaultGame.ini") -Pattern '^ProjectVersion=(.+)$' | Select-Object -First 1
if (-not $VersionLine) { throw "ProjectVersion not found in Config\DefaultGame.ini" }
$Version = $VersionLine.Matches[0].Groups[1].Value.Trim()
$ShortVersion = $Version -replace '\.0$', ''

Write-Host "== Copying the project to $Copy"
New-Item -ItemType Directory -Force $Copy | Out-Null
Copy-Item (Join-Path $Repo "SmashKarts.uproject") $Copy -Force
foreach ($Dir in "Config", "Content", "Source") {
    robocopy (Join-Path $Repo $Dir) (Join-Path $Copy $Dir) /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copying $Dir failed (robocopy $LASTEXITCODE)" }
}
$HasOnline = Test-Path (Join-Path $Copy "Config\Windows\WindowsEngine.ini")

Write-Host "== Building the editor for the copy (the cook needs it)"
& (Join-Path $Batch "Build.bat") SmashKartsEditor Win64 Development "-Project=$Copy\SmashKarts.uproject" -WaitMutex -FromMsBuild
if ($LASTEXITCODE -ne 0) { throw "Editor build failed ($LASTEXITCODE)" }

Write-Host "== Build, cook, stage, pak ($Configuration)"
if (Test-Path $Archive) { Remove-Item $Archive -Recurse -Force }
& (Join-Path $Batch "RunUAT.bat") BuildCookRun "-project=$Copy\SmashKarts.uproject" -noP4 -platform=Win64 "-clientconfig=$Configuration" `
    -build -cook -stage -pak -iostore -archive "-archivedirectory=$Archive" -utf8output -unattended
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed ($LASTEXITCODE)" }

$Game = Join-Path $Archive "Windows"
if (-not (Test-Path (Join-Path $Game "SmashKarts.exe"))) { throw "No SmashKarts.exe in $Game" }
Copy-Item (Join-Path $Repo "Docs\HOW_TO_PLAY.md"), (Join-Path $Repo "Docs\HOSTING.md"), (Join-Path $Repo "Tools\Packaging\Play_Windowed.bat") $Game -Force
if ($Configuration -eq "Shipping") {
    Get-ChildItem $Game -Recurse -Filter *.pdb | Remove-Item -Force # friends don't need debug symbols
}

$Tag = if ($Configuration -eq "Shipping") { "" } else { "dev_" }
$BuildDir = Join-Path $Repo "Build"
$Zip = Join-Path $BuildDir "KartArena_${Tag}v$ShortVersion.zip"
New-Item -ItemType Directory -Force $BuildDir | Out-Null
if (Test-Path $Zip) { Remove-Item $Zip -Force }
Write-Host "== Zipping"
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($Game, $Zip, [IO.Compression.CompressionLevel]::Optimal, $false)

$SizeMB = [math]::Round((Get-Item $Zip).Length / 1MB)
Write-Host ""
Write-Host "Done: $Zip ($SizeMB MB, version $Version, $Configuration)"
if ($HasOnline) {
    Write-Host "Online play: ON (EOS credentials from Config\Windows\WindowsEngine.ini are inside this build)"
} else {
    Write-Host "Online play: OFF (no Config\Windows\WindowsEngine.ini; friends can only join by IP). See Docs\EOS_SETUP.md"
}

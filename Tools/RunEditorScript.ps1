# Runs a Python editor script headless (no editor window) and prints the [SK] lines from its log.
# Usage: powershell -ExecutionPolicy Bypass -File Tools\RunEditorScript.ps1 sk_inspect.py
# The editor must be closed (the script saves assets).
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string]$EngineDir = "C:\Program Files\Epic Games\UE_5.8"
)

$ErrorActionPreference = "Stop"

# An open editor locks assets, and UBT silently builds a hot-reload DLL into it. Refuse to run.
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Host "Unreal Editor is running. Close it first (save your work), then run this again."
    exit 2
}
$Repo = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Repo "SmashKarts.uproject"
$ScriptPath = Join-Path $PSScriptRoot "EditorScripts\$Script"
$LogDir = Join-Path $Repo "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$LogFile = Join-Path $LogDir ("EditorScript_" + [IO.Path]::GetFileNameWithoutExtension($Script) + ".log")
$Cmd = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if (-not (Test-Path $ScriptPath)) { throw "Script not found: $ScriptPath" }

& $Cmd $Project -run=pythonscript "-Script=$ScriptPath" -unattended -nop4 -nosplash "-abslog=$LogFile" | Out-Null
$Code = $LASTEXITCODE

Select-String -Path $LogFile -Pattern "\[SK\]|Error:|LogPython: Error|Traceback|PythonScriptCommandlet" | ForEach-Object { $_.Line }
Write-Host "Exit code: $Code  (log: $LogFile)"
exit $Code

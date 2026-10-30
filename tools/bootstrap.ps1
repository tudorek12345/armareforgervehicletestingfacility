$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path $PSScriptRoot -Parent
$localRoot = Join-Path $workspaceRoot '.local'
$bootstrapRoot = Join-Path $localRoot 'bootstrap'
$scriptRoot = Join-Path $bootstrapRoot 'Scripts\WorkbenchGame'
New-Item -ItemType Directory -Force -Path $scriptRoot | Out-Null
# Official Bohemia sample project identity, used only for the private bootstrap.
# The actual addon gets its own identity from Workbench.GenerateGloballyUniqueID64.
@'
GameProject {
 ID "SampleMod_WorkbenchPlugin"
 GUID "5614E4816F653D1A"
 TITLE "Local playground bootstrap using official Workbench sample"
 Dependencies {
  "58D0FB3206B6F859"
 }
}
'@ | Set-Content -LiteralPath (Join-Path $bootstrapRoot 'addon.gproj') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RVP_Bootstrap.c') -Destination $scriptRoot
$config = Get-Content -LiteralPath (Join-Path $localRoot 'installation.json') -Raw | ConvertFrom-Json
$wb = Join-Path $config.apps.'1874910'.path 'Workbench\ArmaReforgerWorkbenchSteamDiag.exe'
if (-not (Test-Path -LiteralPath $wb)) { throw 'Run python tools/discover.py after installing standard Arma Reforger Tools.' }
$arguments = @('-gproj', ('"' + (Join-Path $bootstrapRoot 'addon.gproj') + '"'),
 '-addonsDir', ('"' + (Join-Path $config.apps.'1874880'.path 'addons') + '"'),
 '-profile', ('"' + (Join-Path $localRoot 'workbench-profile') + '"'),
 '-wbModule=ResourceManager', '-plugin=RVP_Bootstrap', '-run', ('-target="' + $config.project_root + '"'))
$process = Start-Process -FilePath $wb -ArgumentList $arguments -PassThru -WindowStyle Hidden -WorkingDirectory (Split-Path $wb -Parent)
Write-Output "Workbench bootstrap PID $($process.Id)"

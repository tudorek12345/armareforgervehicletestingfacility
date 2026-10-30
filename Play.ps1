param(
 [ValidateSet('Core','Artillery','RHS','WCS')][string]$Integration = 'Core',
 [switch]$Direct
)
$ErrorActionPreference = 'Stop'
$workspaceRoot = $PSScriptRoot
$configPath = Join-Path $workspaceRoot '.local\installation.json'
if (-not (Test-Path -LiteralPath $configPath)) { throw 'Run python tools/discover.py once to discover this PC.' }
$config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
$project = Join-Path $workspaceRoot 'addon'
$addonDirs = @($workspaceRoot)
if ($Integration -ne 'Core') {
 $project = Join-Path $workspaceRoot "integrations\$Integration"
 $addonDirs += (Join-Path $workspaceRoot 'integrations')
 $addonDirs += @($config.addon_roots | Where-Object { Test-Path -LiteralPath $_ })
}
$projectFile = Join-Path $project 'addon.gproj'
if (-not (Test-Path -LiteralPath $projectFile)) { throw "Integration project missing: $Integration" }
$projectText = Get-Content -LiteralPath $projectFile -Raw
$match = [regex]::Match($projectText, '\bGUID\s+"([A-Fa-f0-9]{16})"')
if (-not $match.Success) { throw 'Project does not have a valid engine-generated addon identity.' }
if (-not (Test-Path -LiteralPath $config.game_executable)) { throw 'The discovered game executable no longer exists. Rerun discovery.' }
$arguments = @('-addonsDir', ('"'+($addonDirs -join ',')+'"'), '-addons', $match.Groups[1].Value,
 '-profile', ('"'+(Join-Path $workspaceRoot ".local\game-$Integration")+'"'), '-noSplash')
if ($Direct) {
 $world = 'Worlds/VehiclePlayground.ent'
 if ($Integration -ne 'Core') { $world = "Worlds/RVP_$Integration.ent" }
 $meta = Get-Content -LiteralPath (Join-Path $project ($world+'.meta')) -Raw
 $resource = [regex]::Match($meta, 'Name\s+"([^"]+)"').Groups[1].Value
 $arguments += @('-world', ('"'+$resource+'"'))
}
# Interactive game window: the user needs to see and control it.
Start-Process -FilePath $config.game_executable -ArgumentList $arguments -WorkingDirectory (Split-Path $config.game_executable -Parent) -WindowStyle Normal | Out-Null
if ($Direct) { Write-Output "Starting $Integration playground directly in an isolated local profile." }
else { Write-Output "Starting $Integration project. Choose Scenarios, then REFORGER VEHICLE PLAYGROUND." }

param(
 [ValidateSet('Inspect','Build','Validate','Pack','Open','PlayCheck','Integrations')][string]$Mode = 'Open',
 [ValidateSet('Core','Artillery','RHS','WCS')][string]$Integration = 'Core'
)
$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path $PSScriptRoot -Parent
$localRoot = Join-Path $workspaceRoot '.local'
$config = Get-Content -LiteralPath (Join-Path $localRoot 'installation.json') -Raw | ConvertFrom-Json
$wb = Join-Path $config.apps.'1874910'.path 'Workbench\ArmaReforgerWorkbenchSteamDiag.exe'
if (-not (Test-Path -LiteralPath $wb)) { throw 'Matching standard Workbench is required.' }
$selectedRoot = $config.project_root
$addonDirs = @((Join-Path $config.apps.'1874880'.path 'addons'))
$profileName = 'project-profile'
if ($Integration -ne 'Core') {
 $selectedRoot = Join-Path $workspaceRoot "integrations\$Integration"
 $addonDirs += $workspaceRoot
 $addonDirs += (Join-Path $workspaceRoot 'integrations')
 $addonDirs += @($config.addon_roots | Where-Object { Test-Path -LiteralPath $_ })
 $profileName = "profile-$Integration"
}
$arguments = @('-gproj', ('"' + (Join-Path $selectedRoot 'addon.gproj') + '"'),
 '-addonsDir', ('"' + ($addonDirs -join ',') + '"'),
 '-profile', ('"' + (Join-Path $localRoot $profileName) + '"'), '-scriptAuthorizeAll', '-disableCrashReporter')
switch ($Mode) {
 'Inspect' { $arguments += @('-wbModule=WorldEditor','-plugin=RVP_InspectWorld','-run') }
 'Build' {
  $builder = 'RVP_BuildWorld'
  if ($Integration -ne 'Core') { $builder = 'RVP_BuildIntegration' }
  $arguments += @('-wbModule=WorldEditor',"-plugin=$builder",'-run')
 }
 'Integrations' { $arguments += @('-wbModule=ResourceManager','-plugin=RVP_CreateIntegrations','-run',('-integrationRoot="'+(Join-Path $workspaceRoot 'integrations')+'"')) }
 'PlayCheck' { $arguments += @('-wbModule=WorldEditor','-plugin=RVP_PlayCheck','-run') }
 'Validate' { $arguments += @('-wbModule=ScriptEditor','-validate','PC','-run','-exitAfterInit') }
 'Pack' {
  $packPath = Join-Path $localRoot "packed\$Integration"
  New-Item -ItemType Directory -Path $packPath -Force | Out-Null
  $arguments += @('-wbModule=ResourceManager','-packAddon','-packAddonDir',('"'+$packPath+'"'),'-run','-exitAfterInit')
 }
 'Open' {
  $worldPath = 'Worlds/VehiclePlayground.ent'
  if ($Integration -ne 'Core') { $worldPath = "Worlds/RVP_$Integration.ent" }
  $arguments += @('-wbModule=WorldEditor','-load',$worldPath,'-run')
 }
}
$process = Start-Process -FilePath $wb -ArgumentList $arguments -PassThru -WindowStyle Hidden -WorkingDirectory (Split-Path $wb -Parent)
Write-Output "$Mode PID $($process.Id)"

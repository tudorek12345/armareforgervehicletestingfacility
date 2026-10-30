# REFORGER VEHICLE PLAYGROUND

Local Enfusion addon for the installed **Arma Reforger 1.8.0.13**. The project, generated worlds, and scripts are outside Steam and OneDrive. Existing mod presets and downloaded addons are unchanged.

## Play

From this directory, run:

```powershell
.\Play.ps1
```

In the game, choose **Scenarios → REFORGER VEHICLE PLAYGROUND**. You spawn as a US character beside the arsenal hub at Arland airport. Look at a vehicle's door and use its displayed **Get in** action. Display and test vehicles are assigned to the player faction so the hostile-faction entry restriction does not block them.

To load immediately, use `.\Play.ps1 -Direct`. For the larger installed selection with artillery, use `.\Play.ps1 -Integration WCS -Direct`.

Use the **Open vehicle playground controls** action on the hub arsenal. Close the panel before walking or interacting with vehicles. The panel searches the catalog, spawns at clear pads, repairs/refuels supported vehicles, replaces a selected type, restores a category, resets range targets, clears unoccupied test spawns, and travels between areas. Servicing selects the most recently tracked vehicle of the chosen type.

**Replacement reset** restores the prefab's normal loadout. Universal in-place ammunition replenishment is not implemented; the panel says so. Occupied vehicles are preserved. Cleanup acts only on tracked playground entities.

The enemy base is marked on the map, roughly **1.9 km southeast of the hub**. It has command, barracks and supply tents, sandbag fortifications, and **30 hostile USSR infantry** in six defending squads. The red controls select 20/30/50 opponents, toggle respawning, or remove the enemies. Eliminated squads are scheduled to respawn after 120 seconds; respawning is deferred while the player is within 100 metres. Enemies engage perceived US threats around their base. Normal damage, weapons, physics, and AI detection apply.

## Installed content variants

```powershell
.\Play.ps1 -Integration Artillery
.\Play.ps1 -Integration RHS
.\Play.ps1 -Integration WCS
```

Each command uses a separate local profile and loads only that project's declared dependency set. Select the correspondingly named playground scenario.

| Variant | Additional catalog | Initial additional physical lineup |
| --- | ---: | --- |
| Core | 174 vanilla prefab candidates | 174 editor placements, plus eight resettable empty vehicle targets |
| Artillery | Four CIE artillery variants | Four, including 2A36 and M777 operating/tow variants; compatible shells, charges and ramrods supplied |
| RHS | 94 additional prefab candidates | 20; remaining entries available through the panel |
| WCS | 411 additional candidates, including RHS and CIE artillery | 24; larger categories load on request; compatible artillery ammunition supplied |

These are resource/placement counts, **not a claim that every variant has been driven or fired**. The complete archive inventory, exclusions, dependency graph, and verification evidence are in `reports/`. `placement.tsv` in each integration identifies the physical versus on-demand entries. The WCS and RHS counts overlap; do not add them together.

For large packs, **Restore category** queues additional vehicles gradually. A blocked pad is skipped and logged instead of overwriting another entity. Moving existing vehicles away makes more pad space available. The initial lineup is deliberately smaller for those packs because the vanilla collection already contains 174 entities.

No functional boats or fixed-wing aircraft were confirmed in the selected installed resources. Jets, cruise-missile/airstrike addons, and DonAir's missing terrain were **not downloaded**, following your later instruction. The installed DonAir scenario remains blocked by missing dependency `64770A732AEA31D6` (Battle for Donetsk Airport). No third-party files were patched to hide that dependency. Armed helicopters and the installed artillery retain their own normal weapon systems.

## Locations and operations

The world inherits the official Arland terrain. Its airport features were read from the loaded world; parking candidates were tested against terrain height, slope and existing object bounds. The runway and cross-runway were excluded from editor placement. The hub is approximately `1197, 38, 2832`, the empty target/artillery area approximately `2199, 54, 2712`, and the enemy base approximately `2423, 88, 1414`. Map coordinates are engine X/Y/Z in metres.

Use the airport perimeter roads and island roads for driving, and adjacent open ground for off-road driving. Aircraft parking and map travel points are separate from the runway. There are no objectives, match timer, automatic victory, or invulnerability settings. The enemy base is a live-fire target area; the empty vehicle range is separate.

## Open, rebuild and validate

```powershell
.\tools\workbench.ps1 -Mode Open
.\tools\workbench.ps1 -Mode Build
.\tools\workbench.ps1 -Mode Validate
.\tools\workbench.ps1 -Mode PlayCheck
.\tools\workbench.ps1 -Mode Pack
```

Add `-Integration Artillery`, `RHS`, or `WCS` for the corresponding project. `PlayCheck` runs the actual Workbench game mode and temporarily checks native seat actions, then returns the player to the hub. `Open` opens the saved world for editing. Avoid rebuilding or editing source while playing a Workbench session, because Workbench can reload scripts.

`python tools/discover.py` re-inspects Steam manifests, project dependency metadata and readable archive entries. It does not prove gameplay functionality. `tools/read_installed.py` is a read-only PAC1 inspection helper; it refuses unsupported/protected entries. Engine-generated project/resource identities and native editor serialization are used for the addon itself. No Arma 3 formats are involved.

Machine-specific paths are in ignored `.local/installation.json`. Logs, isolated profiles and packed outputs also stay in `.local/`. The actual project is `addon/addon.gproj`; optional projects are in `integrations/`.

## Verification limits

Workbench script compilation and actual world/player initialization have been run. Native entry checks passed for the Mi-8, BRDM-2, LAV-25, UAZ, M252 mortar and M2 machine gun. Refuel, occupied-reset protection and unoccupied replacement were checked. Removing one owned enemy squad reduced the population to 25; the real respawn timer restored 30. The user confirmed corrected vehicle alignment. Consult `reports/verification.json` for the recorded checks and limits.

The standalone Core and WCS games were launched. WCS registered all 411 additional entries, found 24 additional physical displays and counted 30 live enemies. Native Photo mode and map UI logged errors with this mod set; normal playground startup completed. The intro splash also logs an unknown export-rule class before the game scripts load. These are recorded in the verification report. Three tutorial-specific vehicle variants were excluded after inspection, including the broken M151 and an ambulance variant with missing action managers.

Desktop automation was unavailable. Remaining hands-on acceptance: close the controls panel, enter and drive the UAZ; enter and fly the Mi-8 from its parking; travel to the weapon range, pick up the supplied 81/82 mm mortar shells and use the matching mortar; fire the M2; attack the marked enemy base and check AI response and performance. Mouse interaction, driving, flight, weapon firing and long-session performance have not been verified. Successful placement or a scripted seat action does not prove those operations.

Technical references: [project setup](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Project_Setup), [startup parameters](https://community.bistudio.com/wiki/Arma_Reforger:Startup_Parameters), [entity catalogs](https://community.bistudio.com/wiki/Arma_Reforger:Entity_Catalog), [Workbench plugins](https://community.bistudio.com/wiki/Arma_Reforger:Workbench_Plugin_Tutorial). Installed scripts and prefabs were the primary version-specific reference.


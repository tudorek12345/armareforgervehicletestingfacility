#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "Build vehicle playground", wbModules: {"WorldEditor"})]
class RVP_BuildWorld : WorkbenchPlugin
{
 protected WorldEditor m_Editor;
 protected WorldEditorAPI m_Api;
 protected ref RVP_Clearance m_Clearance = new RVP_Clearance();
 protected ref array<vector> m_Reserved = {};
 protected ref array<float> m_Radii = {};
 protected vector m_Apron;
 protected vector m_RunwayA;
 protected vector m_RunwayB;
 protected vector m_CrossA;
 protected vector m_CrossB;
 protected vector m_Hub;
 protected vector m_Range;
 protected ref map<string, vector> m_Areas = new map<string, vector>();
 protected ref map<string, ResourceName> m_Resources = new map<string, ResourceName>();
 protected float m_MinHubDistance;

 void IndexResource(ResourceName resource)
 {
  string key = resource.GetPath(); key.ToLower(); m_Resources.Set(key, resource);
 }

 ResourceName Canonical(string input)
 {
  ResourceName result;
  string key = input; key.ToLower();
  if (m_Resources.Find(key, result)) return result;
  return input;
 }

 array<ref ContainerIdPathEntry> ComponentPath(IEntitySource source, string className)
 {
  array<ref ContainerIdPathEntry> path = {};
  for (int i = 0; i < source.GetComponentCount(); i++)
   if (source.GetComponent(i).GetClassName() == className)
   { path.Insert(new ContainerIdPathEntry("components", i)); return path; }
  return path;
 }

 float SegmentDistance(vector p, vector a, vector b)
 {
  p[1] = 0; a[1] = 0; b[1] = 0;
  vector ab = b - a;
  float length = ab.LengthSq();
  if (length < 1) return vector.Distance(p, a);
  float t = Math.Clamp(vector.Dot(p - a, ab) / length, 0, 1);
  return vector.Distance(p, a + ab * t);
 }

 bool Reserve(vector p, float radius)
 {
  for (int i = 0; i < m_Reserved.Count(); i++)
   if (vector.Distance(p, m_Reserved[i]) < radius + m_Radii[i] + 3) return false;
  m_Reserved.Insert(p); m_Radii.Insert(radius); return true;
 }

 bool FindSite(vector seed, float radius, out vector p, int maxSteps = 400)
 {
  BaseWorld world = m_Api.GetWorld();
  // Deterministic square rings around an observed terrain feature, sampled in the loaded world.
  int maxRing = 32;
  if (m_MinHubDistance > 0) maxRing = 70;
  for (int ring = 0; ring < maxRing; ring++)
   for (int z = -ring; z <= ring; z++)
    for (int x = -ring; x <= ring; x++)
    {
     if (ring > 0 && Math.AbsInt(x) != ring && Math.AbsInt(z) != ring) continue;
     p = seed + Vector(x * 18, 0, z * 18);
     p[1] = world.GetSurfaceY(p[0], p[2]) + 0.12;
     if (m_MinHubDistance > 0 && vector.Distance(p, m_Hub) < m_MinHubDistance) continue;
     if (SegmentDistance(p, m_RunwayA, m_RunwayB) < radius + 45 || SegmentDistance(p, m_CrossA, m_CrossB) < radius + 45) continue;
     bool reserved;
     for (int r = 0; r < m_Reserved.Count(); r++)
      if (vector.Distance(p, m_Reserved[r]) < radius + m_Radii[r] + 3) { reserved = true; break; }
     if (reserved || !m_Clearance.Check(world, p, radius)) continue;
     Reserve(p, radius); return true;
    }
  return false;
 }

 IEntitySource Create(string prefab, string name, vector p, vector angles = "0 0 0")
 {
  return m_Api.CreateEntity(Canonical(prefab), name, m_Api.GetCurrentEntityLayerId(), null, p, angles);
 }

 void LocateTerrainFeatures()
 {
  int count = m_Api.GetEditorEntityCount();
  for (int i = 0; i < count; i++)
  {
   IEntity entity = m_Api.SourceToEntity(m_Api.GetEditorEntity(i));
   if (!entity || !entity.GetPrefabData()) continue;
   string path = entity.GetPrefabData().GetPrefabName().GetPath();
   if (path.EndsWith("RG_FuelChannel_01.et")) m_Apron = entity.GetOrigin();
   if (path.EndsWith("Runway_Num8.et")) m_RunwayA = entity.GetOrigin();
   if (path.EndsWith("Runway_Num6.et")) m_RunwayB = entity.GetOrigin();
   if (path.EndsWith("Runway_Num4.et") && entity.GetOrigin()[0] < 1000) m_CrossA = entity.GetOrigin();
   if (path.EndsWith("Runway_Num3.et") && entity.GetOrigin()[2] < 2400) m_CrossB = entity.GetOrigin();
  }
 }

 bool CreateSubscene()
 {
  string worldPath = "$ReforgerVehiclePlayground:Worlds/VehiclePlayground.ent";
  FileIO.MakeDirectory("$ReforgerVehiclePlayground:Worlds");
  // Exact subscene/layer serialization used by the installed SCR_PrefabEditingPlugin.
  if (!FileIO.FileExists(worldPath))
  {
   FileHandle file = FileIO.OpenFile(worldPath, FileMode.WRITE);
   if (!file) return false;
   file.WriteLine("SubScene {");
   file.WriteLine(" Parent \"" + Workbench.GetResourceName("$ArmaReforger:worlds/Arland/Arland.ent") + "\"");
   file.WriteLine("}");
   file.WriteLine("Layer default {"); file.WriteLine(" Index 0"); file.WriteLine("}"); file.Close();
   FileIO.MakeDirectory("$ReforgerVehiclePlayground:Worlds/VehiclePlayground_Layers");
   file = FileIO.OpenFile("$ReforgerVehiclePlayground:Worlds/VehiclePlayground_Layers/default.layer", FileMode.WRITE);
   if (!file) return false;
   file.Close();
  }
  ResourceManager rm = Workbench.GetModule(ResourceManager);
  string absolute; Workbench.GetAbsolutePath(worldPath, absolute);
  rm.RegisterResourceFile(absolute, false);
  return m_Editor.SetOpenedResource(worldPath);
 }

 void AddTerminal(IEntitySource source)
 {
  IEntityComponentSource actions = SCR_BaseContainerTools.FindComponentSource(source, ActionsManagerComponent);
  if (!actions) return;
  array<ref ContainerIdPathEntry> path = ComponentPath(source, "ActionsManagerComponent");
  m_Api.CreateObjectArrayVariableMember(source, path, "additionalActions", "RVP_OpenPanelAction", 0);
  FixTerminal(source);
 }

 void FixTerminal(IEntitySource source)
 {
  IEntityComponentSource actions = SCR_BaseContainerTools.FindComponentSource(source, ActionsManagerComponent);
  if (!actions) return;
  BaseContainerList list = actions.GetObjectArray("additionalActions");
  if (!list) return;
  for (int i = 0; i < list.Count(); i++)
  {
   if (list.Get(i).GetClassName() != "RVP_OpenPanelAction") continue;
   array<string> contexts = {"default"};
   list.Get(i).Set("ParentContextList", contexts);
  }
 }

 void GroundSource(IEntitySource source)
 {
  IEntity entity = m_Api.SourceToEntity(source);
  if (!entity) return;
  m_Api.SetVariableValue(source, null, "angles", "0 0 0");
  entity = m_Api.SourceToEntity(source);
  vector lo, hi; entity.GetBounds(lo, hi);
  vector p = entity.GetOrigin(); p[1] = m_Api.GetWorld().GetSurfaceY(p[0], p[2]) - lo[1] + 0.15;
  m_Api.SetVariableValue(source, null, "coords", p.ToString(false));
 }

 void BuildEnemyBase()
 {
  IEntity extraMap = m_Api.GetWorld().FindEntityByName("RVP_Map");
  if (extraMap) m_Api.DeleteEntity(m_Api.EntityToSource(extraMap));
  IEntity hub = m_Api.GetWorld().FindEntityByName("RVP_Travel_Hub");
  if (!hub) return;
  m_Hub = hub.GetOrigin();
  IEntity aiWorld = m_Api.GetWorld().FindEntityByName("RVP_AIWorld");
  if (aiWorld) m_Api.EntityToSource(aiWorld).SetAncestor(Canonical("Prefabs/AI/SCR_AIWorld_Arland.et"));
  LocateTerrainFeatures();
  AddMortarSupplies();
  if (m_Api.GetWorld().FindEntityByName("RVP_EnemyBase")) return;
  m_MinHubDistance = 1600;
  vector basePosition;
  if (!FindSite(m_Hub + Vector(1100, 0, -1400), 8, basePosition))
  { Print("RVP ENEMY: no verified distant base site", LogLevel.ERROR); m_MinHubDistance = 0; return; }
  Create("RVP_EnemyBase", "RVP_EnemyBase", basePosition);
  array<string> structures = {
   "Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et",
   "Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_USSR_01.et",
   "Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_USSR_02.et",
   "Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Supply_Medium_USSR_01.et",
   "Prefabs/Compositions/Misc/SubCompositions/Fortifications/Sandbag_Bunker_Camouflaged_USSR_01.et",
   "Prefabs/Compositions/Misc/SubCompositions/Fortifications/Sandbag_MG_USSR_01_PKM.et",
   "Prefabs/Compositions/Misc/SubCompositions/Fortifications/Sandbag_Wall_USSR_01.et",
   "Prefabs/Compositions/Misc/SubCompositions/Fortifications/Sandbag_Wall_USSR_01.et"
  };
  for (int i = 0; i < structures.Count(); i++)
  {
   vector site;
   if (FindSite(basePosition + Vector((Math.Mod(i, 4) - 2) * 25, 0, (i / 4) * 40), 6, site))
   {
    IEntitySource structure = Create(structures[i], "RVP_EnemyStructure_" + i.ToString(), site);
    if (structure) GroundSource(structure);
   }
  }
  m_MinHubDistance = 0;
  Print("RVP ENEMY: built hostile target base at " + basePosition.ToString(false));
 }

 void AddMortarSupplies()
 {
  if (m_Api.GetWorld().FindEntityByName("RVP_MortarSupply_0")) return;
  IEntity anchor = m_Api.GetWorld().FindEntityByName("RVP_Travel_Weapons");
  if (!anchor) return;
  array<string> supplies = {
   "Prefabs/Weapons/Ammo/Ammo_Shell_81mm_HE_M821.et",
   "Prefabs/Weapons/Ammo/Ammo_Shell_81mm_Illum_M853A1.et",
   "Prefabs/Weapons/Ammo/Ammo_Shell_81mm_Practice_M879.et",
   "Prefabs/Weapons/Ammo/Ammo_Shell_81mm_Smoke_M819.et",
   "Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et",
   "Prefabs/Weapons/Ammo/Ammo_Shell_82mm_Illum_S832S.et",
   "Prefabs/Weapons/Ammo/Ammo_Shell_82mm_Smoke_D832DU.et",
   "Prefabs/Weapons/Ammo/Shell_81mm_Charge_Ring.et",
   "Prefabs/Weapons/Ammo/Shell_82mm_Charge_Ring.et"
  };
  m_MinHubDistance = 950;
  int placed;
  for (int type = 0; type < supplies.Count(); type++)
   for (int copy = 0; copy < 4; copy++)
   {
    vector site;
    if (!FindSite(anchor.GetOrigin() + Vector(type * 2, 0, -12 - copy * 2), 0.7, site)) continue;
    IEntitySource supply = Create(supplies[type], "RVP_MortarSupply_" + (type * 4 + copy).ToString(), site);
    if (supply) { GroundSource(supply); placed++; }
   }
  m_MinHubDistance = 0;
  Print(string.Format("RVP BUILD: %1 compatible mortar shell/charge items placed at the range", placed));
 }

 bool CreateHeader()
 {
  Resource headerResource = BaseContainerTools.CreateContainer("SCR_MissionHeader");
  BaseContainer header = headerResource.GetResource().ToBaseContainer();
  ResourceManager resourceManager = Workbench.GetModule(ResourceManager);
  string absoluteWorld; Workbench.GetAbsolutePath("$ReforgerVehiclePlayground:Worlds/VehiclePlayground.ent", absoluteWorld);
  header.Set("World", resourceManager.GetMetaFile(absoluteWorld).GetResourceID());
  header.Set("m_sName", "REFORGER VEHICLE PLAYGROUND");
  header.Set("m_sAuthor", "tudorek12345");
  header.Set("m_sDescription", "Arland vehicle and crew-weapon sandbox. Spawn at the arsenal hub; use its playground action for travel, resets and servicing.");
  header.Set("m_sDetails", "Normal physics and damage. Clear daytime. Distant enemy base starts with 30 hostile infantry. Panel selects 20/30/50, toggles squad respawning or removes enemies. No objectives or match timer. Optional content uses separate integrations.");
  header.Set("m_sGameMode", "Sandbox"); header.Set("m_iPlayerCount", 1);
  header.Set("m_bShowInScenarioMenu", true); header.Set("m_eSaveTypes", 0);
  header.Set("m_bOverrideScenarioTimeAndWeather", true);
  header.Set("m_iStartingHours", 12); header.Set("m_iStartingMinutes", 0);
  header.Set("m_bRandomStartingDaytime", false); header.Set("m_bRandomStartingWeather", false);
  header.Set("m_bRandomWeatherChanges", false);
  FileIO.MakeDirectory("$ReforgerVehiclePlayground:Missions");
  string path = "$ReforgerVehiclePlayground:Missions/VehiclePlayground.conf";
  if (!BaseContainerTools.SaveContainer(header, ResourceName.Empty, path)) return false;
  ResourceManager rm = Workbench.GetModule(ResourceManager);
  string absolute; Workbench.GetAbsolutePath(path, absolute);
  rm.RegisterResourceFile(absolute, false);
  Resource verified = Resource.Load(path);
  return verified && verified.IsValid();
 }

 override void RunCommandline()
 {
  m_Editor = Workbench.GetModule(WorldEditor); m_Api = m_Editor.GetApi();
  ResourceManager manager = Workbench.GetModule(ResourceManager);
  string layoutAbsolute;
  Workbench.GetAbsolutePath("$ReforgerVehiclePlayground:UI/layouts/RVP_Panel.layout", layoutAbsolute);
  manager.RegisterResourceFile(layoutAbsolute, true);
  SearchResourcesFilter filter = new SearchResourcesFilter();
  filter.fileExtensions = {"et", "ent", "conf"};
  ResourceDatabase.SearchResources(filter, IndexResource);
  array<ResourceName> identityAssets; RVP_InstalledCandidates.Get(identityAssets);
  FileHandle identityFile = FileIO.OpenFile("$profile:resolved-assets.tsv", FileMode.WRITE);
  foreach (ResourceName identityAsset : identityAssets) identityFile.WriteLine(Canonical(identityAsset.GetPath()));
  identityFile.Close();
  if (!CreateSubscene()) { Print("RVP BUILD: subscene failed", LogLevel.ERROR); Workbench.Exit(2); return; }
  if (m_Api.GetWorld().FindEntityByName("RVP_Controller"))
  {
   m_Api.BeginEntityAction("Update playground terminals");
   array<string> terminals = {"RVP_HubTerminal", "RVP_Terminal_Ground", "RVP_Terminal_Armor", "RVP_Terminal_Helicopters", "RVP_Terminal_FixedWing", "RVP_Terminal_Weapons"};
   foreach (string terminalName : terminals)
   {
    IEntity entity = m_Api.GetWorld().FindEntityByName(terminalName);
    if (!entity) continue;
    IEntitySource terminalSource = m_Api.EntityToSource(entity);
    FixTerminal(terminalSource);
   }
   array<ResourceName> existingAssets; RVP_InstalledCandidates.Get(existingAssets);
   for (int e = 0; e < existingAssets.Count(); e++)
   {
    IEntity display = m_Api.GetWorld().FindEntityByName("RVP_Display_" + e.ToString());
    string path = existingAssets[e].GetPath(); path.ToLower();
    if (display && path.Contains("tutorial")) { m_Api.DeleteEntity(m_Api.EntityToSource(display)); continue; }
    if (display) GroundSource(m_Api.EntityToSource(display));
   }
   for (int t = 0; t < 8; t++)
   {
    IEntity target = m_Api.GetWorld().FindEntityByName("RVP_Target_" + t.ToString());
    if (target) GroundSource(m_Api.EntityToSource(target));
   }
   BuildEnemyBase();
   m_Api.EndEntityAction("Update playground terminals");
   m_Editor.Save();
   Print(string.Format("RVP BUILD: existing built world retained; terminal contexts updated; header=%1", CreateHeader()));
   Workbench.Exit(0); return;
  }
  LocateTerrainFeatures();
  if (m_Apron == vector.Zero || m_RunwayA == vector.Zero || m_RunwayB == vector.Zero)
  { Print("RVP BUILD: observed airport anchors missing", LogLevel.ERROR); Workbench.Exit(3); return; }
  if (!FindSite(m_Apron + Vector(0, 0, 100), 9, m_Hub))
  { Print("RVP BUILD: no clear hub", LogLevel.ERROR); Workbench.Exit(4); return; }
  m_MinHubDistance = 1000;
  if (!FindSite(m_Hub + Vector(1200, 0, -300), 6, m_Range))
  { Print("RVP BUILD: no isolated range", LogLevel.ERROR); Workbench.Exit(5); return; }
  m_MinHubDistance = 0;
  m_Api.BeginEntityAction("Build Reforger Vehicle Playground");
  Create("RVP_Playground", "RVP_Controller", m_Hub);
  IEntitySource mode = Create("{1B76F75A3175E85C}Prefabs/MP/Modes/Plain/GameMode_Plain.et", "RVP_GameMode", m_Hub);
  IEntityComponentSource respawn = SCR_BaseContainerTools.FindComponentSource(mode, SCR_RespawnSystemComponent);
  array<ref ContainerIdPathEntry> logicPath = ComponentPath(mode, "SCR_RespawnSystemComponent");
  logicPath.Insert(new ContainerIdPathEntry("m_SpawnLogic"));
  m_Api.SetVariableValue(mode, logicPath, "m_sForcedFaction", "US");
  m_Api.CreateComponent(mode, "SCR_TimeAndWeatherHandlerComponent");
  Create("{589540B35EBA8CDC}Prefabs/MP/Managers/Factions/FactionManager_USxUSSR.et", "RVP_Factions", m_Hub);
  Create("{97EF4C1A41F63DDB}Prefabs/MP/Managers/Loadouts/LoadoutManager_USxUSSR.et", "RVP_Loadouts", m_Hub);
  Create("{E0A05C76552E7F58}Prefabs/AI/SCR_AIWorld.et", "RVP_AIWorld", m_Hub);
  Create("{B8E09FAB91C4ECCD}Prefabs/Systems/Radio/RadioManager.et", "RVP_Radio", m_Hub);
  Create("{028DAEAD63E056BE}Prefabs/World/Game/PerceptionManager.et", "RVP_Perception", m_Hub);
  Create("{F69BC912AC8236F9}Prefabs/MP/ScriptedChatEntity.et", "RVP_Chat", m_Hub);
  Create("Prefabs/MP/Spawning/SpawnPoint_US.et", "RVP_Spawn", m_Hub);
  Create("GenericEntity", "RVP_Travel_Hub", m_Hub);
  vector arsenalPos = m_Hub + Vector(4, 0, 0);
  IEntitySource arsenal = Create("Prefabs/Props/Military/Arsenal/ArsenalBoxes/US/ArsenalBox_US.et", "RVP_HubTerminal", arsenalPos);
  AddTerminal(arsenal);
  IEntityComponentSource arsenalComponent = SCR_BaseContainerTools.FindComponentSource(arsenal, SCR_ArsenalComponent);
  array<string> categories = {"Ground", "Armor", "Helicopters", "FixedWing", "Weapons"};
  array<vector> seeds = {m_Apron + Vector(50,0,130), m_Apron + Vector(220,0,90), m_Apron + Vector(-100,0,100), m_Apron + Vector(-200,0,80), m_Range};
  for (int c = 0; c < categories.Count(); c++)
  {
   if (categories[c] == "Weapons") m_MinHubDistance = 950; else m_MinHubDistance = 0;
   vector site;
   if (!FindSite(seeds[c], 8, site)) continue;
   m_Areas.Insert(categories[c], site);
   Create("GenericEntity", "RVP_Area_" + categories[c], site);
   Create("GenericEntity", "RVP_Travel_" + categories[c], site);
   IEntitySource station = Create("Prefabs/Props/Military/Arsenal/ArsenalBoxes/US/ArsenalBox_US.et", "RVP_Terminal_" + categories[c], site + Vector(3,0,0));
   AddTerminal(station);
  }
  array<ResourceName> assets; RVP_InstalledCandidates.Get(assets);
  FileHandle report = FileIO.OpenFile("$profile:placement.tsv", FileMode.WRITE);
  report.WriteLine("index\tresource\tcategory\tstatus\tposition");
  int placed;
  for (int i = 0; i < assets.Count(); i++)
  {
   string category = RVP_Playground.CategoryFor(assets[i]);
   string path = assets[i].GetPath(); path.ToLower();
   if (path.Contains("tutorial")) { report.WriteLine(i.ToString()+"\t"+assets[i]+"\t"+category+"\tEXCLUDED_TUTORIAL\t"); continue; }
   if (category == "Weapons") m_MinHubDistance = 950; else m_MinHubDistance = 0;
   vector seed;
   if (!m_Areas.Find(category, seed)) { report.WriteLine(i.ToString()+"\t"+assets[i]+"\t"+category+"\tNO_AREA\t"); continue; }
   Resource resource = Resource.Load(assets[i]);
   if (!resource || !resource.IsValid()) { report.WriteLine(i.ToString()+"\t"+assets[i]+"\t"+category+"\tUNRESOLVED\t"); continue; }
   vector site;
   if (!FindSite(seed, RVP_Playground.RadiusFor(category), site))
   { report.WriteLine(i.ToString()+"\t"+assets[i]+"\t"+category+"\tNO_CLEAR_SLOT\t"); continue; }
   IEntitySource source = Create(Canonical(assets[i].GetPath()), "RVP_Display_"+i.ToString(), site);
   if (source) GroundSource(source);
   if (source) { placed++; report.WriteLine(i.ToString()+"\t"+assets[i]+"\t"+category+"\tEDITOR_PLACED\t"+site.ToString(false)); }
   else report.WriteLine(i.ToString()+"\t"+assets[i]+"\t"+category+"\tCREATE_FAILED\t");
  }
  report.Close();
  m_MinHubDistance = 950;
  for (int t = 0; t < 8; t++)
  {
   vector target;
   if (FindSite(m_Range + Vector(0,0,120), 8, target))
    Create("Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et", "RVP_Target_"+t.ToString(), target);
  }
  BuildEnemyBase();
  m_Api.EndEntityAction("Build Reforger Vehicle Playground");
  bool saved = m_Editor.Save();
  bool header = CreateHeader();
  Print(string.Format("RVP BUILD: %1/%2 editor placed; save=%3 header=%4 hub=%5 range=%6", placed, assets.Count(), saved, header, m_Hub, m_Range));
  Workbench.Exit(0);
 }
}
#endif

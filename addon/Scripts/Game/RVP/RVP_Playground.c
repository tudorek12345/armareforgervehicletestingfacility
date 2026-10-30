class RVP_Asset
{
 ResourceName Prefab;
 string Name;
 string Category;
 string Status = "discovered";
}

class RVP_Owned
{
 IEntity Entity;
 ResourceName Prefab;
 string Category;
 vector Home;
 vector Angles;
 vector Transform[4];
 bool Display;
 bool Target;
}

// Terrain and obstruction checks are shared by the editor builder and runtime spawner.
class RVP_Clearance
{
 protected bool m_Blocked;
 protected vector m_Min;
 protected vector m_Max;
 protected IEntity m_Ignore;

 bool CheckEntity(IEntity entity)
 {
  if (!entity || entity == m_Ignore || !entity.GetVObject()) return true;
  IEntity ancestor = entity.GetParent();
  while (ancestor)
  {
   if (ancestor == m_Ignore) return true;
   ancestor = ancestor.GetParent();
  }
  vector lo, hi; entity.GetWorldBounds(lo, hi);
  if (hi[1] <= m_Min[1] || lo[1] >= m_Max[1]) return true;
  if (hi[0] <= m_Min[0] || lo[0] >= m_Max[0] || hi[2] <= m_Min[2] || lo[2] >= m_Max[2]) return true;
  m_Blocked = true;
  return false;
 }

 bool Check(BaseWorld world, vector position, float radius, IEntity ignore = null)
 {
  float ground = world.GetSurfaceY(position[0], position[2]);
  position[1] = ground;
  // Sample the whole footprint, not just the center. Never use this path for boats.
  for (int x = -1; x <= 1; x++)
   for (int z = -1; z <= 1; z++)
    if (Math.AbsFloat(world.GetSurfaceY(position[0] + x * radius, position[2] + z * radius) - ground) > 0.6) return false;
  if (ground < 1) return false;
  m_Min = position - Vector(radius, -0.45, radius);
  m_Max = position + Vector(radius, 15, radius);
  m_Ignore = ignore; m_Blocked = false;
  world.QueryEntitiesByAABB(m_Min, m_Max, CheckEntity);
  return !m_Blocked;
 }
}

class RVP_PlaygroundClass : GenericEntityClass {}

[EntityEditorProps(category: "Reforger Vehicle Playground", description: "Single-player playground controller")]
class RVP_Playground : GenericEntity
{
 static RVP_Playground Instance;
 ref array<ref RVP_Asset> Assets = {};
 ref array<ref RVP_Owned> Owned = {};
 ref RVP_Panel Panel;
 protected ref RVP_Clearance m_Clearance = new RVP_Clearance();
 protected ref array<int> m_Queue = {};
 protected ref array<string> m_Seen = {};
 protected int m_Next;

 static string CategoryFor(ResourceName resource)
 {
  string path = resource.GetPath(); path.ToLower();
  if (path.Contains("helicopter") || path.Contains("/uh1") || path.Contains("/mi8")) return "Helicopters";
  if (path.Contains("/planes/") || path.Contains("/aircraft/") || path.Contains("/jets/") || path.Contains("/fixedwing/")) return "FixedWing";
  if (path.Contains("/boats/") || path.Contains("/watercraft/")) return "Boats";
  if (path.Contains("/tracked/") || path.Contains("/armored/") || path.Contains("/btr") || path.Contains("/lav") || path.Contains("/brdm") || path.Contains("/tank")) return "Armor";
  if (path.Contains("/weapons/") || path.Contains("/static/") || path.Contains("/artillery/") || path.Contains("/howitzer/") || path.Contains("wheeled_2a36") || path.Contains("wheeled_m777")) return "Weapons";
  return "Ground";
 }

 static float RadiusFor(string category)
 {
  if (category == "Helicopters") return 18;
  if (category == "FixedWing") return 28;
  if (category == "Weapons") return 5;
  return 7;
 }

 void AddAsset(ResourceName resource, string displayName = "")
 {
  string key = resource.GetPath(); key.ToLower();
  if (key.Contains("/vehparts/") || key.Contains("/dst/") || key.Contains("_gun_mount") || key.Contains("cinematic_") || key.Contains("/wreck") || key.Contains("tutorial")) return;
  if (m_Seen.Contains(key)) return;
  m_Seen.Insert(key);
  RVP_Asset asset = new RVP_Asset();
  asset.Prefab = resource; asset.Category = CategoryFor(resource);
  if (displayName.IsEmpty()) displayName = FilePath.StripExtension(FilePath.StripPath(resource.GetPath()));
  asset.Name = displayName;
  Assets.Insert(asset);
 }

 void ReadCatalog(SCR_EntityCatalog catalog)
 {
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
   if (entry && entry.IsEnabled()) AddAsset(entry.GetPrefab(), entry.GetEntityName());
 }

 void InitializeCatalog()
 {
  SCR_EntityCatalogManagerComponent manager = SCR_EntityCatalogManagerComponent.GetInstance();
  if (manager)
  {
   ReadCatalog(manager.GetEntityCatalogOfType(EEntityCatalogType.VEHICLE, false));
   ReadCatalog(manager.GetEntityCatalogOfType(EEntityCatalogType.WEAPONS_TRIPOD, false));
   array<Faction> factions = {};
   GetGame().GetFactionManager().GetFactionsList(factions);
   foreach (Faction faction : factions)
   {
    SCR_Faction scrFaction = SCR_Faction.Cast(faction);
    if (!scrFaction) continue;
    ReadCatalog(scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE));
    ReadCatalog(scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.WEAPONS_TRIPOD));
   }
  }
  array<ResourceName> fallback;
  RVP_InstalledCandidates.Get(fallback);
  foreach (ResourceName resource : fallback) AddAsset(resource);
  Print(string.Format("RVP: %1 unique mounted catalog/fallback assets", Assets.Count()));
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode() || Replication.IsClient()) return;
  Instance = this;
  GetGame().GetCallqueue().CallLater(StartPlayground, 1000, false);
 }

 void StartPlayground()
 {
  // Adopt the display as friendly before the distant base establishes US/USSR hostility.
  SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
  array<Faction> allFactions = {}; factionManager.GetFactionsList(allFactions);
  foreach (Faction first : allFactions)
   foreach (Faction second : allFactions)
   {
    SCR_Faction a = SCR_Faction.Cast(first); SCR_Faction b = SCR_Faction.Cast(second);
    if (a && b) factionManager.SetFactionsFriendly(a, b, -1, false);
   }
  InitializeCatalog();
  // Adopt only entities whose addon-authored names are in our reserved namespace.
  // There is no recurring world scan and cleanup never searches for arbitrary wrecks.
  array<ResourceName> baked;
  RVP_InstalledCandidates.Get(baked);
  for (int i = 0; i < baked.Count(); i++)
  {
   string bakedPath = baked[i].GetPath(); bakedPath.ToLower();
   if (bakedPath.Contains("tutorial")) continue;
   IEntity entity = GetWorld().FindEntityByName("RVP_Display_" + i.ToString());
   if (entity) Track(entity, baked[i], CategoryFor(baked[i]), true, false);
  }
  for (int t = 0; t < 8; t++)
  {
   IEntity target = GetWorld().FindEntityByName("RVP_Target_" + t.ToString());
   if (target && target.GetPrefabData()) Track(target, target.GetPrefabData().GetPrefabName(), "Range", false, true);
  }
  GetGame().GetCallqueue().CallLater(AddAreaMarkers, 2000, false);
  Print("RVP: playground ready. Use the arsenal hub's playground action.");
 }

 void AddAreaMarkers()
 {
  if (!GetGame().GetPlayerController()) { GetGame().GetCallqueue().CallLater(AddAreaMarkers, 2000, false); return; }
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (!manager) return;
  array<string> names = {"Hub", "Ground", "Armor", "Helicopters", "FixedWing", "Weapons"};
  foreach (string name : names)
  {
   IEntity anchor = GetWorld().FindEntityByName("RVP_Travel_" + name);
   if (!anchor) continue;
   vector position = anchor.GetOrigin();
   SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
   marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
   marker.SetIconEntry(0); marker.SetColorEntry(0);
   marker.SetCustomText("Playground: " + name);
   marker.SetWorldPos(position[0], position[2]);
   manager.InsertStaticMarker(marker, true);
  }
 }

 void CheckPanelShortcut()
 {
  if (GetGame().GetInputManager().GetActionTriggered("RVP_OpenPanel")) TogglePanel();
 }

 void TogglePanel()
 {
  if (Panel) { Panel.Close(); Panel = null; return; }
  Panel = new RVP_Panel(); Panel.Open(this);
 }

 void Track(IEntity entity, ResourceName resource, string category, bool display, bool target)
 {
  RVP_Owned record = new RVP_Owned();
  record.Entity = entity; record.Prefab = resource; record.Category = category;
  record.Home = entity.GetOrigin(); record.Angles = entity.GetYawPitchRoll();
  entity.GetTransform(record.Transform);
  record.Display = display; record.Target = target; Owned.Insert(record);
  AssignPlayerFaction(entity);
 }

 void AssignPlayerFaction(IEntity entity)
 {
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFactionByKey("US");
 }

 bool OccupiedRecursive(IEntity entity)
 {
  if (!entity) return false;
  SCR_BaseCompartmentManagerComponent manager = SCR_BaseCompartmentManagerComponent.Cast(entity.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (manager)
  {
   array<IEntity> occupants = {}; manager.GetOccupants(occupants);
   if (!occupants.IsEmpty()) return true;
  }
  IEntity child = entity.GetChildren();
  while (child)
  {
   if (OccupiedRecursive(child)) return true;
   child = child.GetSibling();
  }
  return false;
 }

 bool FindPad(string category, out vector position)
 {
  IEntity anchor = GetWorld().FindEntityByName("RVP_Area_" + category);
  if (!anchor || category == "Boats") return false;
  vector center = anchor.GetOrigin();
  float radius = RadiusFor(category);
  for (int i = 0; i < 60; i++)
  {
   position = center + Vector(Math.Mod(i, 6) * radius * 2.4, 0, (i / 6) * radius * 2.4);
   position[1] = GetWorld().GetSurfaceY(position[0], position[2]) + 0.1;
   if (m_Clearance.Check(GetWorld(), position, radius)) return true;
  }
  return false;
 }

 string SpawnAsset(int index)
 {
  if (index < 0 || index >= Assets.Count()) return "Select an asset first.";
  RVP_Asset asset = Assets[index];
  Resource resource = Resource.Load(asset.Prefab);
  if (!resource || !resource.IsValid()) return "Missing resource/dependency: " + asset.Prefab;
  asset.Status = "resource-resolved";
  vector position;
  if (!FindPad(asset.Category, position)) return "No verified clear pad for " + asset.Category + ". Move vehicles away from its test pads.";
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.MatrixIdentity4(parameters.Transform); parameters.Transform[3] = position;
  IEntity entity = GetGame().SpawnEntityPrefab(resource, GetWorld(), parameters);
  if (!entity) return "Engine refused to spawn " + asset.Prefab;
  GroundEntity(entity);
  Track(entity, asset.Prefab, asset.Category, false, false);
  asset.Status = "spawned; operation untested";
  Refuel(entity);
  return "Spawned " + asset.Name + " at " + position.ToString(false) + ". Uses its normal prefab loadout.";
 }

 static void GroundEntity(IEntity entity)
 {
  vector lo, hi; entity.GetBounds(lo, hi);
  vector origin = entity.GetOrigin();
  origin[1] = entity.GetWorld().GetSurfaceY(origin[0], origin[2]) - lo[1] + 0.15;
  entity.SetOrigin(origin); entity.Update();
 }

 string Refuel(IEntity entity)
 {
  FuelManagerComponent fuel = FuelManagerComponent.Cast(entity.FindComponent(FuelManagerComponent));
  if (!fuel) return "This asset has no supported fuel manager.";
  array<BaseFuelNode> nodes = {}; fuel.GetFuelNodesList(nodes);
  foreach (BaseFuelNode node : nodes) node.SetFuel(node.GetMaxFuel());
  return "Refueled supported fuel tanks.";
 }

 RVP_Owned FindSelected(int assetIndex)
 {
  if (assetIndex < 0 || assetIndex >= Assets.Count()) return null;
  for (int i = Owned.Count() - 1; i >= 0; i--)
   if (Owned[i].Entity && Owned[i].Prefab.GetPath() == Assets[assetIndex].Prefab.GetPath()) return Owned[i];
  return null;
 }

 string Service(int index, int operation)
 {
  RVP_Owned record = FindSelected(index);
  if (!record || !record.Entity) return "No tracked vehicle of the selected type is present.";
  if (OccupiedRecursive(record.Entity)) return "Vehicle occupied. Exit all seats before servicing/reset.";
  if (operation == 1) return Refuel(record.Entity);
  if (operation == 0)
  {
   SCR_VehicleDamageManagerComponent damage = SCR_VehicleDamageManagerComponent.Cast(record.Entity.FindComponent(SCR_VehicleDamageManagerComponent));
   if (!damage) return "No supported vehicle repair component. Use replacement reset.";
   damage.FullHeal(); return "Applied the normal vehicle component's FullHeal repair.";
  }
  if (operation == 2) return "In-place rearm is not universal. Replacement reset restores this prefab's normal supported ammunition/loadout.";
  return Replace(record);
 }

 string Replace(RVP_Owned record)
 {
  if (record.Entity && OccupiedRecursive(record.Entity)) return "Skipped occupied vehicle.";
  // Check before deletion so a blocked home pad cannot destroy the current vehicle.
  if (!m_Clearance.Check(GetWorld(), record.Home, RadiusFor(record.Category), record.Entity)) return "Reset pad blocked; vehicle preserved.";
  Resource resource = Resource.Load(record.Prefab);
  if (!resource || !resource.IsValid()) return "Reset resource missing; vehicle preserved.";
  if (record.Entity) SCR_EntityHelper.DeleteEntityAndChildren(record.Entity);
  EntitySpawnParams parameters = new EntitySpawnParams(); parameters.TransformMode = ETransformMode.WORLD;
  for (int axis = 0; axis < 4; axis++) parameters.Transform[axis] = record.Transform[axis];
  record.Entity = GetGame().SpawnEntityPrefab(resource, GetWorld(), parameters);
  if (!record.Entity) return "Engine rejected replacement; inspect resource log.";
  AssignPlayerFaction(record.Entity);
  Refuel(record.Entity); return "Replaced with normal undamaged prefab/loadout.";
 }

 string Cleanup()
 {
  int removed;
  for (int i = Owned.Count() - 1; i >= 0; i--)
  {
   RVP_Owned record = Owned[i];
   if (record.Entity && OccupiedRecursive(record.Entity)) continue;
   if (record.Display || record.Target)
   {
    DamageManagerComponent damage;
    if (record.Entity) damage = DamageManagerComponent.Cast(record.Entity.FindComponent(DamageManagerComponent));
    if (!damage || damage.GetState() != EDamageState.DESTROYED) continue;
    SCR_EntityHelper.DeleteEntityAndChildren(record.Entity); record.Entity = null; removed++;
    continue;
   }
   if (record.Entity) SCR_EntityHelper.DeleteEntityAndChildren(record.Entity);
   Owned.Remove(i); removed++;
  }
  return string.Format("Cleared %1 unoccupied playground wrecks/test spawns. Live displays preserved; use Restore category or Reset targets to replace cleared wrecks.", removed);
 }

 string Restore(string category, bool targets = false)
 {
  int restored; int skipped;
  foreach (RVP_Owned record : Owned)
  {
   if ((targets && record.Target) || (!targets && record.Display && record.Category == category))
   {
    if (Replace(record).StartsWith("Replaced")) restored++; else skipped++;
   }
  }
  if (!targets)
  {
   GetGame().GetCallqueue().Remove(SpawnQueuedDisplay);
   m_Queue.Clear(); m_Next = 0;
   for (int i = 0; i < Assets.Count(); i++)
   {
    if (Assets[i].Category != category) continue;
    bool displayed;
    foreach (RVP_Owned existing : Owned)
     if (existing.Display && existing.Entity && existing.Prefab.GetPath() == Assets[i].Prefab.GetPath()) { displayed = true; break; }
    if (!displayed) m_Queue.Insert(i);
   }
   if (!m_Queue.IsEmpty()) GetGame().GetCallqueue().CallLater(SpawnQueuedDisplay, 250, false);
  }
  return string.Format("Restored %1, skipped %2 occupied/blocked entries. %3 additional category assets queued at clear pads.", restored, skipped, m_Queue.Count());
 }

 void SpawnQueuedDisplay()
 {
  if (m_Next >= m_Queue.Count()) return;
  int before = Owned.Count();
  string outcome = SpawnAsset(m_Queue[m_Next++]);
  if (Owned.Count() > before) Owned[Owned.Count() - 1].Display = true;
  else Print("RVP CATEGORY: " + outcome, LogLevel.WARNING);
  if (m_Next < m_Queue.Count()) GetGame().GetCallqueue().CallLater(SpawnQueuedDisplay, 250, false);
 }

 string Travel(string area)
 {
  IEntity player = GetGame().GetPlayerController().GetControlledEntity();
  if (!player) return "No controllable player.";
  ChimeraCharacter character = ChimeraCharacter.Cast(player);
  if (!character || character.IsInVehicle()) return "Exit your vehicle before travel.";
  IEntity anchor = GetWorld().FindEntityByName("RVP_Travel_" + area);
  if (!anchor) return "Travel point unavailable: " + area;
  vector destination = anchor.GetOrigin();
  if (!m_Clearance.Check(GetWorld(), destination, 0.6, player)) return "Travel destination blocked.";
  player.SetOrigin(destination); player.Update();
  return "Travelled to " + area;
 }

 void RVP_Playground(IEntitySource src, IEntity parent) { SetEventMask(EntityEvent.INIT); }
 void ~RVP_Playground()
 {
  if (GetGame()) { GetGame().GetCallqueue().Remove(CheckPanelShortcut); GetGame().GetCallqueue().Remove(SpawnQueuedDisplay); }
  if (Panel) Panel.Close();
  if (Instance == this) Instance = null;
 }
}

class RVP_OpenPanelAction : ScriptedUserAction
{
 override bool HasLocalEffectOnlyScript() { return true; }
 override bool GetActionNameScript(out string outName) { outName = "Open vehicle playground controls"; return true; }
 override bool CanBeShownScript(IEntity user) { return RVP_Playground.Instance != null; }
 override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 {
  if (RVP_Playground.Instance) RVP_Playground.Instance.TogglePanel();
 }
}

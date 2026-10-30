class RVP_EnemySquad
{
 SCR_AIGroup Group;
 AIWaypoint Waypoint;
 vector Position;
 float EmptySince = -1;
}

class RVP_EnemyBaseClass : GenericEntityClass {}

[EntityEditorProps(category: "Reforger Vehicle Playground", description: "Distant hostile target base with a bounded infantry population")]
class RVP_EnemyBase : GenericEntity
{
 static RVP_EnemyBase Instance;
 protected ref array<ref RVP_EnemySquad> m_Squads = {};
 protected int m_Strength = 30;
 protected bool m_Enabled = true;
 protected bool m_Respawn = true;
 protected float m_Seconds;

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode() || Replication.IsClient()) return;
  Instance = this;
  GetGame().GetCallqueue().CallLater(Start, 3500, false);
 }

 void Start()
 {
  SetStrength(m_Strength);
  GetGame().GetCallqueue().CallLater(Tick, 10000, true);
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (manager && GetGame().GetPlayerController())
  {
   SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
   marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM); marker.SetIconEntry(0); marker.SetColorEntry(0);
   marker.SetCustomText("ENEMY BASE: live hostile infantry");
   vector position = GetOrigin(); marker.SetWorldPos(position[0], position[2]); manager.InsertStaticMarker(marker, true);
  }
 }

 void MakeHostile()
 {
  SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
  if (!manager) return;
  SCR_Faction playerFaction = SCR_Faction.Cast(manager.GetFactionByKey("US"));
  SCR_Faction enemyFaction = SCR_Faction.Cast(manager.GetFactionByKey("USSR"));
  if (playerFaction && enemyFaction) manager.SetFactionsHostile(playerFaction, enemyFaction, -1, false);
 }

 IEntity Spawn(ResourceName prefab, vector position)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource || !resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams(); parameters.TransformMode = ETransformMode.WORLD;
  Math3D.MatrixIdentity4(parameters.Transform); parameters.Transform[3] = position;
  return GetGame().SpawnEntityPrefab(resource, GetWorld(), parameters);
 }

 void SpawnSquad(RVP_EnemySquad squad)
 {
  squad.Group = SCR_AIGroup.Cast(Spawn("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", squad.Position));
  if (!squad.Group) { Print("RVP ENEMY: group prefab failed", LogLevel.ERROR); return; }
  squad.Group.m_faction = "USSR";
  squad.Group.SetSpawnImmediately(false);
  squad.Group.SetMemberSpawnDelay(200);
  squad.Group.m_aUnitPrefabSlots = {
   "{5436629450D8387A}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_SL.et",
   "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et",
   "{8E0FE664CE7D1CA9}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_GL.et",
   "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et",
   "{333DA6244C7DA34C}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_SR.et"
  };
  squad.Group.SpawnUnits();
  squad.Waypoint = AIWaypoint.Cast(Spawn("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", squad.Position));
  if (squad.Waypoint) squad.Group.AddWaypoint(squad.Waypoint);
  squad.EmptySince = -1;
 }

 int Alive(RVP_EnemySquad squad)
 {
  if (!squad.Group) return 0;
  array<AIAgent> agents = {}; squad.Group.GetAgents(agents);
  int alive;
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   ChimeraCharacter unit = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (unit && unit.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD) alive++;
  }
  return alive;
 }

 void ClearSquad(RVP_EnemySquad squad)
 {
  if (squad.Group)
  {
   array<AIAgent> agents = {}; squad.Group.GetAgents(agents);
   IEntity player;
   if (GetGame().GetPlayerController()) player = GetGame().GetPlayerController().GetControlledEntity();
   foreach (AIAgent agent : agents)
   {
    if (!agent) continue;
    IEntity entity = agent.GetControlledEntity();
    if (entity && entity != player) SCR_EntityHelper.DeleteEntityAndChildren(entity);
   }
   SCR_EntityHelper.DeleteEntityAndChildren(squad.Group); squad.Group = null;
  }
  if (squad.Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(squad.Waypoint);
  squad.Waypoint = null;
 }

 string SetStrength(int strength)
 {
  if (strength != 20 && strength != 30 && strength != 50) return "Choose 20, 30 or 50 opponents.";
  GetGame().GetCallqueue().Remove(SpawnSquad);
  foreach (RVP_EnemySquad old : m_Squads) ClearSquad(old);
  m_Squads.Clear(); m_Strength = strength; m_Enabled = true;
  MakeHostile();
  for (int i = 0; i < strength / 5; i++)
  {
   RVP_EnemySquad squad = new RVP_EnemySquad();
   squad.Position = GetOrigin() + Vector(Math.Mod(i, 5) * 14 - 28, 0, (i / 5) * 25 - 12);
   squad.Position[1] = GetWorld().GetSurfaceY(squad.Position[0], squad.Position[2]) + 0.1;
   m_Squads.Insert(squad);
   GetGame().GetCallqueue().CallLater(SpawnSquad, 500 + i * 700, false, squad);
  }
  Print(string.Format("RVP ENEMY: requested %1 hostile USSR infantry at %2; respawn=%3", strength, GetOrigin(), m_Respawn));
  return string.Format("Enemy base: %1 hostile infantry, arriving in small squads. Respawn delay: 120 seconds after a squad is eliminated.", strength);
 }

 string Disable()
 {
  m_Enabled = false;
  GetGame().GetCallqueue().Remove(SpawnSquad);
  foreach (RVP_EnemySquad squad : m_Squads) ClearSquad(squad);
  m_Squads.Clear();
  return "Enemy infantry removed from this addon's base. Base structures retained.";
 }

 string ToggleRespawn()
 {
  m_Respawn = !m_Respawn;
  if (m_Respawn) return "Enemy squad respawning ON: 120 seconds after elimination, delayed if you are within 100 metres.";
  return "Enemy respawning OFF. Current opponents remain.";
 }

 string Status()
 {
  int alive;
  foreach (RVP_EnemySquad squad : m_Squads) alive += Alive(squad);
  return string.Format("Enemy base: %1 alive / %2 target, enabled=%3, respawn=%4", alive, m_Strength, m_Enabled, m_Respawn);
 }

#ifdef WORKBENCH
 // Exercise the real empty-squad recovery timer without shooting near the player.
 void CheckEmptySquadRecovery()
 {
  if (m_Squads.IsEmpty()) return;
  ClearSquad(m_Squads[0]);
  Print("RVP ENEMY CHECK: removed one owned squad; " + Status());
 }
#endif

 void Tick()
 {
  m_Seconds += 10;
  if (!m_Enabled) return;
  foreach (RVP_EnemySquad squad : m_Squads)
  {
   if (Alive(squad) > 0) { squad.EmptySince = -1; continue; }
   if (squad.EmptySince < 0) squad.EmptySince = m_Seconds;
   if (!m_Respawn || m_Seconds - squad.EmptySince < 120) continue;
   IEntity player;
   if (GetGame().GetPlayerController()) player = GetGame().GetPlayerController().GetControlledEntity();
   if (player && vector.Distance(player.GetOrigin(), squad.Position) < 100) continue;
   ClearSquad(squad); SpawnSquad(squad);
  }
  if (m_Seconds <= 20 || Math.Mod(m_Seconds, 60) == 0) Print("RVP ENEMY: " + Status());
 }

 void RVP_EnemyBase(IEntitySource src, IEntity parent) { SetEventMask(EntityEvent.INIT); }
 void ~RVP_EnemyBase()
 {
  if (GetGame()) { GetGame().GetCallqueue().Remove(Tick); GetGame().GetCallqueue().Remove(SpawnSquad); }
  if (Instance == this) Instance = null;
 }
}

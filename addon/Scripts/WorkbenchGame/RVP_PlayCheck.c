#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "Run playground load check", wbModules: {"WorldEditor"})]
class RVP_PlayCheck : WorldEditorPlugin
{
 protected int m_Attempts;
 override void RunCommandline()
 {
  WorldEditor editor = Workbench.GetModule(WorldEditor);
  string id; Workbench.GetGameProjectSettings().Get("ID", id);
  string worldPath = "$ReforgerVehiclePlayground:Worlds/VehiclePlayground.ent";
  if (id != "ReforgerVehiclePlayground") worldPath = "$" + id + ":Worlds/" + id + ".ent";
  if (!editor.SetOpenedResource(worldPath))
  { Print("RVP PLAYCHECK: world failed to open", LogLevel.ERROR); Workbench.Exit(2); return; }
  editor.SwitchToGameMode();
 }

 override void OnGameModeStarted(string worldName, string gameMode, bool playFromCameraPos, vector cameraPosition, vector cameraAngles)
 {
  Print("RVP PLAYCHECK: entered game mode " + worldName);
  GetGame().GetCallqueue().CallLater(Check, 5000, false);
 }

 void Check()
 {
  m_Attempts++;
  PlayerController controller = GetGame().GetPlayerController();
  IEntity player;
  if (controller) player = controller.GetControlledEntity();
  if (!player && m_Attempts < 12) { GetGame().GetCallqueue().CallLater(Check, 5000, false); return; }
  int present;
  array<ResourceName> assets; RVP_InstalledCandidates.Get(assets);
  for (int i = 0; i < assets.Count(); i++)
   if (GetGame().GetWorld().FindEntityByName("RVP_Display_" + i.ToString())) present++;
  Print(string.Format("RVP PLAYCHECK: player=%1 controllableCharacter=%2 controller=%3 bakedDisplayEntities=%4", player, ChimeraCharacter.Cast(player) != null, RVP_Playground.Instance != null, present));
  if (player) Print("RVP PLAYCHECK: player position " + player.GetOrigin().ToString(false));
  if (RVP_Playground.Instance)
  {
   RVP_Playground.Instance.TogglePanel();
   Print(string.Format("RVP PLAYCHECK: native panel object=%1", RVP_Playground.Instance.Panel != null));
   RVP_Playground.Instance.TogglePanel();
   Print("RVP PLAYCHECK: panel closed; normal player input restored");
  }
  array<int> samples = {0, 19, 30, 120};
  foreach (int sample : samples)
  {
   IEntity vehicle = GetGame().GetWorld().FindEntityByName("RVP_Display_" + sample.ToString());
   if (vehicle) Print(string.Format("RVP PLAYCHECK: sample=%1 position=%2 yawPitchRoll=%3", sample, vehicle.GetOrigin(), vehicle.GetYawPitchRoll()));
  }
  if (player) CheckGetInActions(player);
  if (RVP_EnemyBase.Instance) GetGame().GetCallqueue().CallLater(CheckEnemyRecovery, 8000, false);
  // Deliberately leave the actual playable session open for the user.
 }

 protected ref array<int> m_EntrySamples = {0, 19, 30, 120, 171, 176};
 protected int m_EntryIndex;
 protected SCR_GetInUserAction m_EntryAction;
 protected IEntity m_EntryVehicle;
 protected vector m_EntryReturn;
 protected int m_ExitAttempts;

 void CheckGetInActions(IEntity player)
 {
  m_EntryReturn = player.GetOrigin();
  NextEntryCheck();
 }

 void NextEntryCheck()
 {
  IEntity player = GetGame().GetPlayerController().GetControlledEntity();
  if (!player) return;
  ChimeraCharacter character = ChimeraCharacter.Cast(player);
  if (character && character.IsInVehicle())
  {
   if (m_ExitAttempts++ > 5) { Print("RVP ENTRY: exit did not complete; stopping checks without moving an occupied player", LogLevel.WARNING); return; }
   character.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false);
   GetGame().GetCallqueue().CallLater(NextEntryCheck, 2500, false); return;
  }
  m_ExitAttempts = 0;
  if (m_EntryIndex >= m_EntrySamples.Count())
  {
   player.SetOrigin(m_EntryReturn); player.Update();
   Print("RVP ENTRY: seat checks finished; player returned to hub");
   CheckService();
   return;
  }
  m_EntryVehicle = GetGame().GetWorld().FindEntityByName("RVP_Display_" + m_EntrySamples[m_EntryIndex].ToString());
  m_EntryAction = FindEntryAction(m_EntryVehicle, m_EntrySamples[m_EntryIndex] >= 170);
  if (!m_EntryAction) { Print("RVP ENTRY: pilot action unavailable"); m_EntryIndex++; NextEntryCheck(); return; }
  vector p = m_EntryAction.GetCompartmentSlot().GetPosition() + Vector(-2, 0, 0);
  p[1] = GetGame().GetWorld().GetSurfaceY(p[0], p[2]) + 0.1;
  player.SetOrigin(p); player.Update();
  GetGame().GetCallqueue().CallLater(TryEntry, 1000, false);
 }

 SCR_GetInUserAction FindEntryAction(IEntity entity, bool anySeat)
 {
  ActionsManagerComponent manager = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
  array<BaseUserAction> actions = {}; if (manager) manager.GetActionsList(actions);
  foreach (BaseUserAction candidate : actions)
  {
   SCR_GetInUserAction action = SCR_GetInUserAction.Cast(candidate);
   if (action && action.GetCompartmentSlot() && (anySeat || action.GetCompartmentSlot().GetType() == ECompartmentType.PILOT)) return action;
  }
  IEntity child = entity.GetChildren();
  while (child)
  {
   SCR_GetInUserAction found = FindEntryAction(child, anySeat);
   if (found) return found;
   child = child.GetSibling();
  }
  return null;
 }

 void TryEntry()
 {
  IEntity player = GetGame().GetPlayerController().GetControlledEntity();
  int door = m_EntryAction.GetCompartmentSlot().PickDoorIndexForPoint(player.GetOrigin());
  m_EntryAction.SetCompartmentDoorInfoIndex(door);
  bool allowed = m_EntryAction.CanBePerformedScript(player);
  Print(string.Format("RVP ENTRY: sample=%1 allowed=%2 door=%3 reason=%4", m_EntrySamples[m_EntryIndex], allowed, door, m_EntryAction.GetCannotPerformReason()));
  if (allowed) m_EntryAction.PerformAction(m_EntryAction.GetOwner(), player);
  GetGame().GetCallqueue().CallLater(CheckEntryResult, 5000, false);
 }

 void CheckEntryResult()
 {
  ChimeraCharacter player = ChimeraCharacter.Cast(GetGame().GetPlayerController().GetControlledEntity());
  bool entered;
  if (player && player.IsInVehicle() && player.GetCompartmentAccessComponent().GetCompartment())
   entered = player.GetCompartmentAccessComponent().GetCompartment().GetVehicle() == m_EntryVehicle;
  Print(string.Format("RVP ENTRY: sample=%1 normalActionEnteredVehicle=%2", m_EntrySamples[m_EntryIndex], entered));
  if (entered && RVP_Playground.Instance)
   foreach (RVP_Owned record : RVP_Playground.Instance.Owned)
    if (record.Entity == m_EntryVehicle) Print("RVP SERVICE CHECK: occupied reset=" + RVP_Playground.Instance.Replace(record));
  if (entered) player.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false);
  if (m_EntryAction) m_EntryAction.SetCompartmentDoorInfoIndex(-1);
  m_EntryIndex++;
  GetGame().GetCallqueue().CallLater(NextEntryCheck, 1500, false);
 }

 void CheckService()
 {
  RVP_Playground playground = RVP_Playground.Instance;
  if (!playground) return;
  IEntity vehicle = GetGame().GetWorld().FindEntityByName("RVP_Display_120");
  if (!vehicle) return;
  FuelManagerComponent fuel = FuelManagerComponent.Cast(vehicle.FindComponent(FuelManagerComponent));
  if (fuel)
  {
   array<BaseFuelNode> nodes = {}; fuel.GetFuelNodesList(nodes);
   foreach (BaseFuelNode node : nodes) node.SetFuel(node.GetMaxFuel() * 0.1);
   playground.Refuel(vehicle);
   bool full = !nodes.IsEmpty();
   foreach (BaseFuelNode node : nodes) if (node.GetFuel() < node.GetMaxFuel() - 0.01) full = false;
   Print(string.Format("RVP SERVICE CHECK: UAZ fuel restored to maximum=%1", full));
  }
  foreach (RVP_Owned record : playground.Owned)
   if (record.Entity == vehicle) { Print("RVP SERVICE CHECK: UAZ unoccupied replacement=" + playground.Replace(record)); break; }
 }

 void CheckEnemyRecovery()
 {
  if (!RVP_EnemyBase.Instance) return;
  RVP_EnemyBase.Instance.CheckEmptySquadRecovery();
  GetGame().GetCallqueue().CallLater(ReportEnemyRecovery, 145000, false);
 }

 void ReportEnemyRecovery()
 {
  if (RVP_EnemyBase.Instance) Print("RVP ENEMY CHECK: after 145 seconds, " + RVP_EnemyBase.Instance.Status());
 }
}
#endif

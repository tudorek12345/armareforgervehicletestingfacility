// Small native widget panel; no custom menu registration or speculative layout resource.
class RVP_Panel : ScriptedWidgetEventHandler
{
 protected RVP_Playground m_Playground;
 protected Widget m_Root;
 protected EditBoxWidget m_Search;
 protected SCR_ListBoxComponent m_List;
 protected TextWidget m_Status;
 protected ref array<int> m_Indices = {};
 protected ref map<Widget, string> m_Commands = new map<Widget, string>();

 void Open(RVP_Playground playground)
 {
  m_Playground = playground;
  m_Root = GetGame().GetWorkspace().CreateWidgets("{05705C74FB926E87}UI/layouts/RVP_Panel.layout");
  if (!m_Root) { Print("RVP: panel layout unavailable", LogLevel.ERROR); return; }
  m_Search = EditBoxWidget.Cast(m_Root.FindAnyWidget("Search"));
  m_List = SCR_ListBoxComponent.Cast(m_Root.FindAnyWidget("AssetList").FindHandler(SCR_ListBoxComponent));
  m_Status = TextWidget.Cast(m_Root.FindAnyWidget("Status"));
  m_Status.SetTextWrapping(true);
  array<string> commands = {"spawn", "repair", "refuel", "rearm", "reset", "restore", "targets", "cleanup", "hub", "close", "go_Ground", "go_Armor", "go_Helicopters", "go_FixedWing", "go_Weapons", "enemy20", "enemy30", "enemy50", "respawn", "enemyoff"};
  foreach (string command : commands)
   m_Commands.Insert(m_Root.FindAnyWidget("Command_" + command), command);
  m_Root.AddHandler(this);
  GetGame().GetWorkspace().AddModal(m_Root, m_Search);
  Refresh();
 }

 void Refresh()
 {
  m_List.Clear(); m_Indices.Clear();
  string query = m_Search.GetText(); query.ToLower();
  foreach (int index, RVP_Asset asset : m_Playground.Assets)
  {
   string text = "[" + asset.Category + "] " + WidgetManager.Translate(asset.Name);
   string lower = text + " " + asset.Prefab; lower.ToLower();
   if (!query.IsEmpty() && !lower.Contains(query)) continue;
   m_List.AddItem(text); m_Indices.Insert(index);
  }
  if (!m_Indices.IsEmpty()) m_List.SetItemSelected(0, true);
 }

 int Selected()
 {
  int row = m_List.GetSelectedItem();
  if (row < 0 || row >= m_Indices.Count()) return -1;
  return m_Indices[row];
 }

 override bool OnChange(Widget w, bool finished)
 {
  if (w == m_Search) { Refresh(); return true; }
  return false;
 }

 override bool OnClick(Widget w, int x, int y, int button)
 {
  Widget widget = w;
  string command;
  while (widget && !m_Commands.Find(widget, command)) widget = widget.GetParent();
  if (!widget) return false;
  int selected = Selected();
  string result;
  if (command == "close") { m_Playground.TogglePanel(); return true; }
  if (command == "spawn") result = m_Playground.SpawnAsset(selected);
  if (command == "repair") result = m_Playground.Service(selected, 0);
  if (command == "refuel") result = m_Playground.Service(selected, 1);
  if (command == "rearm") result = m_Playground.Service(selected, 2);
  if (command == "reset") result = m_Playground.Service(selected, 3);
  if (command == "targets") result = m_Playground.Restore("Range", true);
  if (command == "cleanup") result = m_Playground.Cleanup();
  if (command == "hub") result = m_Playground.Travel("Hub");
  if (command.StartsWith("enemy") || command == "respawn")
  {
   RVP_EnemyBase enemyBase = RVP_EnemyBase.Instance;
   if (!enemyBase) result = "Enemy base is unavailable in this world.";
   else if (command == "enemyoff") result = enemyBase.Disable();
   else if (command == "respawn") result = enemyBase.ToggleRespawn();
   else if (command == "enemy20") result = enemyBase.SetStrength(20);
   else if (command == "enemy30") result = enemyBase.SetStrength(30);
   else if (command == "enemy50") result = enemyBase.SetStrength(50);
  }
  if (command.StartsWith("go_")) result = m_Playground.Travel(command.Substring(3, command.Length() - 3));
  if (command == "restore")
  {
   if (selected < 0) result = "Select an asset/category first.";
   else result = m_Playground.Restore(m_Playground.Assets[selected].Category);
  }
  m_Status.SetText(result); return true;
 }

 void Close()
 {
  if (!m_Root) return;
  GetGame().GetWorkspace().RemoveModal(m_Root);
  m_Root.RemoveHandler(this); m_Root.RemoveFromHierarchy(); m_Root = null;
 }
}

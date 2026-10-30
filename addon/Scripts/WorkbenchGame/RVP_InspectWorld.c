#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "Inspect playground terrain", wbModules: {"WorldEditor"})]
class RVP_InspectWorld : WorkbenchPlugin
{
 override void RunCommandline()
 {
  WorldEditor editor = Workbench.GetModule(WorldEditor);
  if (!editor.SetOpenedResource("worlds/Arland/Arland.ent"))
  {
   Print("RVP: terrain load failed", LogLevel.ERROR);
   Workbench.Exit(1);
   return;
  }
  WorldEditorAPI api = editor.GetApi();
  FileHandle file = FileIO.OpenFile("$profile:terrain-sites.tsv", FileMode.WRITE);
  file.WriteLine("prefab\tname\tposition\tangles");
  int count = api.GetEditorEntityCount();
  for (int i = 0; i < count; i++)
  {
   IEntitySource src = api.GetEditorEntity(i);
   IEntity entity = api.SourceToEntity(src);
   if (!entity) continue;
   string prefab;
   if (entity.GetPrefabData()) prefab = entity.GetPrefabData().GetPrefabName();
   string lower = prefab; lower.ToLower();
   if (lower.Contains("runway") || lower.Contains("airfield") || lower.Contains("hangar") || lower.Contains("pier") || lower.Contains("airport"))
    file.WriteLine(prefab + "\t" + src.GetName() + "\t" + entity.GetOrigin().ToString(false) + "\t" + entity.GetYawPitchRoll().ToString(false));
  }
  file.Close();
  Print(string.Format("RVP: inspected %1 actual editor entities", count));
  Workbench.Exit(0);
 }
}
#endif

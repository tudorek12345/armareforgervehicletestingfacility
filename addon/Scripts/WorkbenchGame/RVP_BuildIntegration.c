#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "Build active optional integration", wbModules: {"WorldEditor"})]
class RVP_BuildIntegration : RVP_BuildWorld
{
 void AddArtillerySupplies()
 {
  if (m_Api.GetWorld().FindEntityByName("RVP_ArtilleryAmmo_0")) return;
  vector origin = m_Api.GetWorld().FindEntityByName("RVP_Travel_Weapons").GetOrigin();
  array<string> supplies = {"Prefabs/Weapons/Ammo/Ammo_Shell_152mm_HE_OF29.et", "Prefabs/Weapons/Ammo/Ammo_Powder_152mm_4zh47(Full).et", "Prefabs/Weapons/Ammo/Ammo_Shell_155mm_HE_M107.et", "Prefabs/Weapons/Ammo/Ammo_Powder_155mm_M231.et", "Prefabs/Weapons/Ammo/Attach_US_ramrod.et"};
  m_Api.BeginEntityAction("Supply compatible artillery ammunition");
  for (int type = 0; type < supplies.Count(); type++)
   for (int copy = 0; copy < 4; copy++)
   {
    vector p = origin + Vector(10 + type * 2, 0, copy * 2);
    p[1] = m_Api.GetWorld().GetSurfaceY(p[0], p[2]);
    IEntitySource source = Create(supplies[type], "RVP_ArtilleryAmmo_" + (type * 4 + copy).ToString(), p);
    if (source) GroundSource(source);
   }
  m_Api.EndEntityAction("Supply compatible artillery ammunition");
  m_Editor.Save();
 }

 override void RunCommandline()
 {
  string projectId; Workbench.GetGameProjectSettings().Get("ID", projectId);
  string prefix = "$" + projectId + ":";
  m_Editor = Workbench.GetModule(WorldEditor); m_Api = m_Editor.GetApi();
  SearchResourcesFilter filter = new SearchResourcesFilter(); filter.fileExtensions = {"et", "ent", "conf"};
  ResourceDatabase.SearchResources(filter, IndexResource);
  FileIO.MakeDirectory(prefix + "Worlds"); FileIO.MakeDirectory(prefix + "Missions");
  string worldPath = prefix + "Worlds/" + projectId + ".ent";
  if (!FileIO.FileExists(worldPath))
  {
   FileHandle file = FileIO.OpenFile(worldPath, FileMode.WRITE);
   file.WriteLine("SubScene {");
   file.WriteLine(" Parent \"{E9B55395F9126376}Worlds/VehiclePlayground.ent\"");
   file.WriteLine("}"); file.WriteLine("Layer default {"); file.WriteLine(" Index 0"); file.WriteLine("}"); file.Close();
   FileIO.MakeDirectory(prefix + "Worlds/" + projectId + "_Layers");
   file = FileIO.OpenFile(prefix + "Worlds/" + projectId + "_Layers/default.layer", FileMode.WRITE); file.Close();
  }
  ResourceManager rm = Workbench.GetModule(ResourceManager);
  string absolute; Workbench.GetAbsolutePath(worldPath, absolute); rm.RegisterResourceFile(absolute, false);
  if (!m_Editor.SetOpenedResource(worldPath)) { Workbench.Exit(2); return; }
  if (m_Api.GetWorld().FindEntityByName("RVP_IntegrationContent"))
  {
   if (projectId == "RVP_Artillery" || projectId == "RVP_WCS") AddArtillerySupplies();
   Print("RVP INTEGRATION: existing overlay preserved, supply update complete"); Workbench.Exit(0); return;
  }
  LocateTerrainFeatures();
  m_Hub = m_Api.GetWorld().FindEntityByName("RVP_Travel_Hub").GetOrigin();
  m_Range = m_Api.GetWorld().FindEntityByName("RVP_Travel_Weapons").GetOrigin();
  FileHandle input = FileIO.OpenFile(prefix + "assets.txt", FileMode.READ);
  array<ResourceName> assets = {};
  string line;
  while (input && input.ReadLine(line) >= 0)
   if (!line.IsEmpty()) assets.Insert(Canonical(line));
  if (input) input.Close();
  m_Api.BeginEntityAction("Build optional playground overlay");
  IEntitySource registry = Create("RVP_IntegrationEntity", "RVP_IntegrationContent", m_Hub);
  registry.Set("m_aAssets", assets);
  int placed;
  FileHandle coverage = FileIO.OpenFile(prefix + "placement.tsv", FileMode.WRITE);
  coverage.WriteLine("resource\tcategory\tstatus\tposition");
  for (int i = 0; i < assets.Count(); i++)
  {
   string category = RVP_Playground.CategoryFor(assets[i]);
   if (assets[i].GetPath().Contains("Howitzer")) category = "Weapons";
   Resource resource = Resource.Load(assets[i]);
   if (!resource || !resource.IsValid()) { coverage.WriteLine(assets[i]+"\t"+category+"\tUNRESOLVED\t"); continue; }
   vector seed = m_Range;
   if (category != "Weapons")
   {
    IEntity area = m_Api.GetWorld().FindEntityByName("RVP_Area_" + category);
    if (!area) { coverage.WriteLine(assets[i]+"\t"+category+"\tNO_AREA\t"); continue; }
    seed = area.GetOrigin();
   }
   // Full artillery set; larger vehicle packs start with a category sample to keep the existing 177-vehicle world usable.
   bool artilleryAsset = assets[i].GetPath().Contains("Howitzer") || assets[i].GetPath().Contains("Wheeled_2A36") || assets[i].GetPath().Contains("Wheeled_M777");
   if (projectId != "RVP_Artillery" && placed >= 20 && !artilleryAsset) { coverage.WriteLine(assets[i]+"\t"+category+"\tON_DEMAND\t"); continue; }
   if (category == "Weapons") m_MinHubDistance = 950; else m_MinHubDistance = 0;
   vector site;
   if (!FindSite(seed, RVP_Playground.RadiusFor(category), site)) { coverage.WriteLine(assets[i]+"\t"+category+"\tNO_CLEAR_SLOT\t"); continue; }
   IEntitySource source = Create(assets[i], "RVP_ModDisplay_"+i.ToString(), site);
   if (source) { GroundSource(source); placed++; coverage.WriteLine(assets[i]+"\t"+category+"\tEDITOR_PLACED\t"+site.ToString(false)); }
   else coverage.WriteLine(assets[i]+"\t"+category+"\tCREATE_FAILED\t");
  }
  coverage.Close();
  m_Api.EndEntityAction("Build optional playground overlay"); m_Editor.Save();
  if (projectId == "RVP_Artillery" || projectId == "RVP_WCS") AddArtillerySupplies();
  Resource headerResource = BaseContainerTools.CreateContainer("SCR_MissionHeader");
  BaseContainer header = headerResource.GetResource().ToBaseContainer();
  header.Set("World", rm.GetMetaFile(absolute).GetResourceID());
  header.Set("m_sName", "REFORGER VEHICLE PLAYGROUND - " + projectId);
  header.Set("m_sAuthor", "tudorek12345"); header.Set("m_iPlayerCount", 1);
  header.Set("m_sDescription", "Optional installed content. Read placement.tsv for the physical lineup and on-demand entries.");
  header.Set("m_bShowInScenarioMenu", true); header.Set("m_eSaveTypes", 0);
  header.Set("m_bOverrideScenarioTimeAndWeather", true); header.Set("m_iStartingHours", 12);
  string headerPath = prefix + "Missions/" + projectId + ".conf";
  BaseContainerTools.SaveContainer(header, ResourceName.Empty, headerPath);
  Workbench.GetAbsolutePath(headerPath, absolute); rm.RegisterResourceFile(absolute, false);
  Print(string.Format("RVP INTEGRATION: %1 assets; %2 editor placed; overlay saved", assets.Count(), placed));
  Workbench.Exit(0);
 }
}
#endif

#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "Playground project bootstrap", wbModules: {"ResourceManager"})]
class RVP_Bootstrap : WorkbenchPlugin
{
 override void RunCommandline()
 {
  ResourceManager rm = Workbench.GetModule(ResourceManager);
  string target;
  rm.GetCmdLine("-target", target);
  if (target.IsEmpty()) System.GetCLIParam("target", target);
  if (target.IsEmpty())
  {
   Print("RVP: missing -target", LogLevel.ERROR);
   Workbench.Exit(1);
   return;
  }
  FileIO.MakeDirectory(target);
  if (FileIO.FileExists(target + "/addon.gproj"))
  {
   Print("RVP: preserving existing project identity");
   Workbench.Exit(0);
   return;
  }
  string guid = Workbench.GenerateGloballyUniqueID64();
  FileHandle file = FileIO.OpenFile(target + "/addon.gproj", FileMode.WRITE);
  if (!file) { Workbench.Exit(2); return; }
  file.WriteLine("GameProject {");
  file.WriteLine(" ID \"ReforgerVehiclePlayground\"");
  file.WriteLine(" GUID \"" + guid + "\"");
  file.WriteLine(" TITLE \"REFORGER VEHICLE PLAYGROUND\"");
  file.WriteLine(" Dependencies {");
  file.WriteLine("  \"58D0FB3206B6F859\"");
  file.WriteLine(" }");
  file.WriteLine("}");
  file.Close();
  Print("RVP: created project using engine-generated GUID " + guid);
  Workbench.Exit(0);
 }
}
#endif

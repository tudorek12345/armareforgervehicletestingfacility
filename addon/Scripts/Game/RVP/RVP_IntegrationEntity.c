class RVP_IntegrationEntityClass : GenericEntityClass {}

[EntityEditorProps(category: "Reforger Vehicle Playground", description: "Optional assets supplied by this integration only")]
class RVP_IntegrationEntity : GenericEntity
{
 [Attribute("", UIWidgets.ResourceNamePicker, "Resolved optional prefabs", "et")]
 ref array<ResourceName> m_aAssets;

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode() || Replication.IsClient()) return;
  GetGame().GetCallqueue().CallLater(RegisterAssets, 2500, false);
 }

 void RegisterAssets()
 {
  if (!RVP_Playground.Instance || !m_aAssets) return;
  int placed;
  foreach (int i, ResourceName asset : m_aAssets)
  {
   RVP_Playground.Instance.AddAsset(asset);
   IEntity entity = GetWorld().FindEntityByName("RVP_ModDisplay_" + i.ToString());
   if (!entity) continue;
   RVP_Playground.Instance.Track(entity, asset, RVP_Playground.CategoryFor(asset), true, false); placed++;
  }
  Print(string.Format("RVP INTEGRATION: %1 assets registered; %2 physically placed; remaining entries available at clear pads through the panel.", m_aAssets.Count(), placed));
 }

 void RVP_IntegrationEntity(IEntitySource src, IEntity parent) { SetEventMask(EntityEvent.INIT); }
}

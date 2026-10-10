"""Read-only inventory of playable face geometry, head frames and native Toon APIs."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/ToonImprovement/Inventory'
OUT.mkdir(parents=True,exist_ok=True)
LIB=unreal.MaterialEditingLibrary
EAL=unreal.EditorAssetLibrary
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report=dict(status='RUNNING',characters=[],native={})
source_models=json.loads((ROOT/'Tools/Art/PlayerModels/manifest.json').read_text(encoding='utf-8'))['characters']

def vec(v):return [v.x,v.y,v.z]

for path in EAL.list_assets('/Game/DataCenter/Characters',recursive=False):
    if not path.rsplit('/',1)[-1].startswith('DA_'):continue
    appearance=unreal.load_asset(path)
    if not isinstance(appearance,unreal.PGCharacterAppearance):continue
    mesh=appearance.get_editor_property('mesh')
    if not mesh:continue
    actor=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector())
    component=actor.skeletal_mesh_component
    component.set_skeletal_mesh_asset(mesh)
    head=component.get_socket_transform(appearance.head_bone,unreal.RelativeTransformSpace.RTS_COMPONENT)
    row=dict(id=str(appearance.id),appearance=path,mesh=mesh.get_path_name(),head_bone=str(appearance.head_bone),
        head_origin=vec(head.translation),head_transform=head.export_text(),
        head_forward=vec(unreal.MathLibrary.transform_direction(head,appearance.head_forward_axis)),
        head_right=vec(unreal.MathLibrary.transform_direction(head,appearance.head_right_axis)),materials=[])
    for index,slot in enumerate(mesh.materials):
        material=slot.material_interface
        name=str(slot.material_slot_name)
        parent=material.get_editor_property('parent') if isinstance(material,unreal.MaterialInstanceConstant) else material
        tex=LIB.get_material_instance_texture_parameter_value(material,'BaseTexture')
        entry=dict(index=index,slot=name,material=material.get_path_name(),parent=parent.get_path_name(),texture=tex.get_path_name() if tex else None)
        source_model=next((m for m in source_models if m['name']==str(appearance.id) or (str(appearance.id)=='Hichi' and m['name']=='Yura')),None)
        original_name=source_model['materials'].get(name.removeprefix('PG_'),{}).get('name',name) if source_model else name
        entry['original_name']=original_name
        if any(t in original_name.lower() for t in ['face','head','body','hair','skin']):
            dest=OUT/str(appearance.id)/(str(index)+'.json')
            dest.parent.mkdir(parents=True,exist_ok=True)
            if unreal.PGEditorProbeTools.export_skeletal_material_geometry(mesh,name,str(dest)):
                entry['geometry']=str(dest)
        row['materials'].append(entry)
    report['characters'].append(row)
    actors.destroy_actor(actor)
report['native']={
    'substrate':unreal.SystemLibrary.get_console_variable_int_value('r.Substrate'),
    'gbuffer':unreal.SystemLibrary.get_console_variable_int_value('r.Substrate.ProjectGBufferFormat'),
    'front_material_enum':hasattr(unreal.MaterialProperty,'MP_FRONT_MATERIAL'),
    'toon_node':hasattr(unreal,'MaterialExpressionSubstrateToonBSDF'),
    'toon_factory':hasattr(unreal,'ToonProfileFactory'),
    'material_properties':[n for n in dir(unreal.MaterialProperty) if n.startswith('MP_')],
}
report['status']='PASS'
(OUT/'inventory.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report['native']))

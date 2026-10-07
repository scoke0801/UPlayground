"""Export actual UE Bokusei face LOD0/UV0 and its reference head frame, read-only."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT/'Saved/BokuseiFaceSDF/Model'
OUT.mkdir(parents=True, exist_ok=True)
appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_Bokusei')
mesh = appearance.get_editor_property('mesh')
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector())
component = actor.skeletal_mesh_component
component.set_skeletal_mesh_asset(mesh)
head_name = appearance.get_editor_property('head_bone')
head = component.get_socket_transform(head_name, unreal.RelativeTransformSpace.RTS_COMPONENT)

def vector(value):
    return [value.x, value.y, value.z]

report = dict(status='RUNNING', mesh=mesh.get_path_name(), head_bone=str(head_name),
              head_transform=head.export_text(), head_origin=vector(head.translation),
              head_forward_axis=vector(appearance.head_forward_axis),
              head_right_axis=vector(appearance.head_right_axis), materials=[], protected={})
report['head_forward'] = vector(unreal.MathLibrary.transform_direction(head, appearance.head_forward_axis))
report['head_right'] = vector(unreal.MathLibrary.transform_direction(head, appearance.head_right_axis))
for asset in [appearance, mesh]+[slot.material_interface for slot in mesh.materials]:
    path = ROOT/'Content'/(asset.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset')
    report['protected'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
for index, slot in enumerate(mesh.materials):
    material = slot.material_interface
    texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(material, 'BaseTexture')
    report['materials'].append(dict(index=index, slot=str(slot.material_slot_name), material=material.get_path_name(),
                                    texture=texture.get_path_name() if texture else None))
geometry = OUT/'face_geometry.json'
assert unreal.PGEditorProbeTools.export_skeletal_material_geometry(mesh, 'Mat_Bokusei_Face', str(geometry))
report.update(status='PASS', geometry=str(geometry), geometry_sha256=hashlib.sha256(geometry.read_bytes()).hexdigest())
assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest() == h for p,h in report['protected'].items())
(OUT/'model.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))

"""Build a head-local static occluder from source-derived coarse forelock hulls."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
data=json.loads((ROOT/'Tools/Art/ToonTest/HairShadowProxy/proxy.json').read_text())
DEST='/Game/Art/ToonTest/Improvement/Hair'
EAL=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
path=DEST+'/SM_PGBokusei_HairShadow'
mesh=unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset('/Engine/BasicShapes/Cube',path)
appearance=unreal.load_asset('/Game/DataCenter/Characters/DA_Bokusei')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actor=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector())
actor.skeletal_mesh_component.set_skeletal_mesh_asset(appearance.get_editor_property('mesh'))
head=actor.skeletal_mesh_component.get_socket_transform(appearance.head_bone,unreal.RelativeTransformSpace.RTS_COMPONENT)
description=unreal.StaticMesh.create_static_mesh_description(mesh)
group=description.create_polygon_group()
ids=[]
for point in data['vertices']:
    vertex=description.create_vertex()
    description.set_vertex_position(vertex,unreal.MathLibrary.inverse_transform_location(head,unreal.Vector(*point)))
    ids.append(vertex)
for triangle in data['triangles']:
    instances=[description.create_vertex_instance(ids[i]) for i in triangle]
    for instance in instances:description.set_vertex_instance_uv(instance,unreal.Vector2D(0,0),0)
    description.create_triangle(group,instances)
mesh.build_from_static_mesh_descriptions([description],False,False)
mesh.set_material(0,unreal.load_asset('/Engine/EngineMaterials/DefaultMaterial'))
assert EAL.save_loaded_asset(mesh,only_if_is_dirty=False)
actors.destroy_actor(actor)
report=dict(status='PASS',mesh=mesh.get_path_name(),triangles=len(data['triangles']),vertices=len(data['vertices']))
(ROOT/'Saved/ToonImprovement/hair-proxy.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))

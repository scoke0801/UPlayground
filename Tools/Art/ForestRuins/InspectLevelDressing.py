"""Read-only dimensions/material inventory and current test map actor snapshot."""
import json
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root/'Saved/LevelDressing'
out.mkdir(exist_ok=True)
def vec(v): return [round(getattr(v, a), 2) for a in ('x','y','z')]
report = dict(meshes={}, actors=[])
base = root/'Content/ExternalAssets/LevelDesign'
needed_dungeon={'SM_Floor-Ceiling','SM_Urn_01','SM_Metal_Chest'}
for file in sorted(p for p in base.rglob('*.uasset') if p.stem.lower().startswith('sm_')
        and (('RuinedCrypt' in p.parts and 'Environment' in p.parts) or
             ('Dungeon_Pack' in p.parts and p.stem in needed_dungeon))):
    if 'FoliageType' in file.stem: continue
    path = '/Game/'+file.relative_to(root/'Content').with_suffix('').as_posix()
    mesh = u.load_asset(path)
    if not isinstance(mesh, u.StaticMesh): continue
    box = mesh.get_bounding_box()
    report['meshes'][file.stem] = dict(path=path, min=vec(box.min), max=vec(box.max),
        materials=[str(m.material_interface.get_path_name()) if m.material_interface else None for m in mesh.static_materials])
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/L_PG_ForestRuins')
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    row = dict(name=a.get_actor_label(), cls=a.get_class().get_name(), location=vec(a.get_actor_location()))
    if isinstance(a,u.StaticMeshActor): row['mesh']=str(a.static_mesh_component.static_mesh.get_path_name())
    report['actors'].append(row)
(out/'inventory.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('DRESSING INVENTORY SAVED')

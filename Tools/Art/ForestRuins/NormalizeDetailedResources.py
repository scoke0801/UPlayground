"""Export only LOD0 from individual owned FBX resources, never demo placements."""
import bpy
import json
from pathlib import Path
import re
KIT=Path(__file__).resolve().parent
data=json.loads((KIT/'detailed_sources.json').read_text(encoding='utf-8'))
out=KIT/'DetailedSource/Normalized'
out.mkdir(exist_ok=True)
report={}
for entry in data['meshes']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(KIT/entry['file']))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    names=[o.name for o in meshes]
    keep=[o for o in meshes if not re.search(r'(?i)lod[_ ]?[1-9]',o.name)]
    assert keep,(entry['name'],names)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in keep:obj.select_set(True)
    bpy.context.view_layer.objects.active=keep[0]
    report[entry['name']]=dict(objects=names,kept=[o.name for o in keep],triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in keep),materials=list(dict.fromkeys(m.name for o in keep for m in o.data.materials if m)))
    bpy.ops.export_scene.fbx(filepath=str(out/('SM_PGFR_HD_'+entry['name']+'.fbx')),use_selection=True,
        object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False)
    print('PG_DETAIL',entry['name'],report[entry['name']],flush=True)
(KIT/'detailed_geometry.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

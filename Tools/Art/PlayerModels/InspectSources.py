import json
from pathlib import Path
import bpy

ROOT = Path(__file__).resolve().parents[3]
sources = json.loads(Path(__file__).with_name('sources.json').read_text(encoding='utf-8'))
index = {g:p for c in sources['characters'] for g,p in c['sources'].items()}
result = {}
for guid in ['7e79ae6db10cb6c4b9c17d80e8b7ec68', '39c0da9fedf072243a69f3b09eca8cd6',
             '4c45b161908ac6147b0b1bfc8a6bd197', '3439d6d2474bbc744a568ca510802e97', '49c0ab47e0da7464fb47e2e1d430d358']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=index[guid], use_anim=False)
    rows=[]
    for obj in bpy.data.objects:
        if obj.type != 'MESH': continue
        used=sorted({p.material_index for p in obj.data.polygons})
        arm=obj.find_armature()
        rows.append(dict(name=obj.name, materials=[obj.data.materials[i].name for i in used],
                         used=used, armature=arm.name if arm else None,
                         bounds=[list(obj.matrix_world @ __import__('mathutils').Vector(v)) for v in obj.bound_box],
                         shapes=[k.name for k in obj.data.shape_keys.key_blocks][1:] if obj.data.shape_keys else []))
    result[guid]=dict(path=index[guid], meshes=rows,
        rigs=[dict(name=o.name, matrix=[list(r) for r in o.matrix_world],
                   bones=[dict(name=b.name, parent=b.parent.name if b.parent else None, head=list(b.head_local)) for b in o.data.bones])
              for o in bpy.data.objects if o.type=='ARMATURE'])
(ROOT/'Saved/PlayerModelFBXInspection.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('INSPECTION COMPLETE')

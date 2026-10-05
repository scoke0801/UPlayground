"""Read original FBX geometry in an isolated Blender process; never save sources."""
import hashlib
import json
from pathlib import Path
import bpy

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/MoonlitUI/ModelReferences'
OUT.mkdir(parents=True, exist_ok=True)
spec = json.loads((ROOT / 'Tools/Art/ToonTest/characters.json').read_text())
sources = {r['name']: Path(spec['unity_root']) / r['folder'] / r['fbx'] for r in spec['characters']}
batch = json.loads((ROOT / 'Tools/Art/ToonCharacters/manifest.json').read_text(encoding='utf-8'))
sources.update({{'Suiha': 'Siuha', 'lili': 'Lili'}.get(r['name'], r['name']): Path(r['fbx'])
                for r in batch['characters'] if r['name'] != 'Spi_Reien'})
report = {}
for name, path in sources.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    before = hashlib.sha256(path.read_bytes()).hexdigest()
    bpy.ops.import_scene.fbx(filepath=str(path), use_anim=False)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    report[name] = dict(fbx=str(path), sha256=before,
        meshes=[dict(name=o.name, vertices=len(o.data.vertices), polygons=len(o.data.polygons),
                     materials=[m.name if m else None for m in o.data.materials]) for o in meshes],
        armatures=[dict(name=o.name, bones=len(o.data.bones)) for o in bpy.data.objects if o.type == 'ARMATURE'])
    assert meshes and report[name]['armatures'], name
    assert hashlib.sha256(path.read_bytes()).hexdigest() == before
    (OUT / 'fbx-inspection.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('FBX INSPECTED', name, len(meshes), flush=True)
assert len(report) == 7

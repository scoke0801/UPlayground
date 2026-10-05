"""Compose visible prefab parts, material overrides and default morphs in Blender."""
import hashlib
import json
import re
import sys
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
SELECTED=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
DATA=json.loads((OUT/('manifest.json' if SELECTED else 'sources.json')).read_text(encoding='utf-8'))


def material_map(spec,row,slot):
    candidates=row['materials'];name=re.sub(r'\.\d{3}$','',slot)
    if len(candidates)==1:return candidates[0]
    special={'Acc':'Metal','H':'deep purple','R':'R_black','hichi_face_base':'Hichi_face_BaseColor','hichi_face_op':'Hichi_face_opas'}
    wanted=special.get(name,name).lower()
    matches=[g for g in candidates if spec['materials'][g]['name'].lower()==wanted]
    if not matches: matches=[g for g in candidates if spec['materials'][g]['name'].lower().startswith(wanted+'_')]
    assert len(matches)==1,(row['name'],slot,[spec['materials'][g]['name'] for g in candidates])
    return matches[0]


def build(spec):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    parts=[];rigs=[];audit=[];bones={}
    for guid,path in spec['sources'].items():
        before=set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=path,use_anim=False)
        imported=set(bpy.data.objects)-before
        lookup={re.sub(r'\.\d{3}$','',o.name):o for o in imported if o.type=='MESH'}
        rows=[r for r in spec['renderers'] if r['source_guid']==guid]
        rig=next(o for o in imported if o.type=='ARMATURE')
        if spec['name']=='Yura' and 'head' in rig.data.bones:
            # Unreal bone identifiers are case insensitive: Head and head
            # need distinct names even though Blender accepts both.
            rig.data.bones['head'].name='PG_HairRoot'
            for obj in imported:
                if obj.type=='MESH' and 'head' in obj.vertex_groups:obj.vertex_groups['head'].name='PG_HairRoot'
        rigs.append(rig)
        source_bones={b.name:dict(matrix=rig.matrix_world@b.matrix_local,
            length=b.length*rig.matrix_world.to_scale().length/(3**.5),parent=b.parent.name if b.parent else None) for b in rig.data.bones}
        # The prefab's bone list maps additional clothing/hair joints to the
        # character hierarchy. Shared bones retain the primary FBX bind pose.
        for n,b in source_bones.items():
            if n not in bones: bones[n]=b
        for row in rows:
            obj=lookup[row['name']]
            used=sorted({p.material_index for p in obj.data.polygons})
            old=list(obj.data.materials);indices=[p.material_index for p in obj.data.polygons]
            mapped=[material_map(spec,row,old[i].name) for i in used]
            obj.data.materials.clear()
            for g in mapped:
                name='PG_'+g
                obj.data.materials.append(bpy.data.materials.get(name) or bpy.data.materials.new(name))
                spec['bindings'][name]=g
            for p,i in zip(obj.data.polygons,indices):p.material_index=used.index(i)
            applied=[]
            if obj.data.shape_keys:
                keys=list(obj.data.shape_keys.key_blocks)
                for i,v in enumerate(row['weights']):
                    if v:
                        assert i+1<len(keys),(row['name'],i)
                        keys[i+1].value=v/100
                        applied.append(dict(index=i,name=keys[i+1].name,value=v))
                # Bake only prefab defaults into the base while retaining the
                # remaining expression deltas relative to the new base.
                basis=keys[0]
                delta=[Vector((0,0,0)) for _ in obj.data.vertices]
                for k in keys[1:]:
                    if k.value:
                        for i,v in enumerate(k.data):delta[i]+=(v.co-basis.data[i].co)*k.value
                for k in keys:
                    for i,v in enumerate(k.data):v.co+=delta[i]
                    k.value=0
            parts.append(obj)
            audit.append(dict(name=row['name'],material_guids=mapped,morph_defaults=applied,vertices=len(obj.data.vertices)))
        for obj in imported:
            if obj.type=='MESH' and obj not in parts:bpy.data.objects.remove(obj,do_unlink=True)
    # All FBXs use compatible character-space bind poses. Create one rig so
    # clothing follows shared body joints without duplicate skeleton roots.
    rig_data=bpy.data.armatures.new('PG_'+spec['name'])
    merged=bpy.data.objects.new('root',rig_data);bpy.context.collection.objects.link(merged)
    bpy.context.view_layer.objects.active=merged;merged.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for n,b in bones.items():
        eb=rig_data.edit_bones.new(n);eb.matrix=b['matrix'];eb.length=max(.005,b['length'])
    for n,b in bones.items():
        parent=b['parent']
        if parent:rig_data.edit_bones[n].parent=rig_data.edit_bones[parent]
    # The replacement hair root is already at the head bind position in its
    # source FBX; parenting preserves that world transform.
    if spec['name']=='Yura':rig_data.edit_bones['PG_HairRoot'].parent=rig_data.edit_bones['Head']
    bpy.ops.object.mode_set(mode='OBJECT')
    for obj in parts:
        world=obj.matrix_world.copy();obj.parent=merged;obj.matrix_world=world
        for mod in obj.modifiers:
            if mod.type=='ARMATURE':mod.object=merged
    for rig in rigs:bpy.data.objects.remove(rig,do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT');merged.select_set(True)
    for obj in parts:obj.select_set(True)
    target=OUT/'FBX'/('SK_PG_PlayerModel_'+spec['name']+'.fbx');target.parent.mkdir(exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'MESH','ARMATURE'},
        add_leaf_bones=False,bake_anim=False,use_mesh_modifiers=False,mesh_smooth_type='FACE',
        axis_forward='-Y',axis_up='Z',path_mode='STRIP')
    spec['fbx']=str(target);spec['parts_audit']=audit
    DATA['source_hashes'][str(target)]=hashlib.sha256(target.read_bytes()).hexdigest()
    print('EXPORTED',spec['name'],len(parts),'parts',len(bones),'bones',flush=True)


for character in DATA['characters']:
    if not SELECTED or character['name'] in SELECTED:build(character)
(OUT/'manifest.json').write_text(json.dumps(DATA,ensure_ascii=False,indent=2),encoding='utf-8')

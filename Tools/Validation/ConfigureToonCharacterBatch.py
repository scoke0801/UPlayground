"""Import five base characters and bind the existing world-lit PG toon shader."""
import hashlib
import json
import os
import re
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
import ConfigureToonCharacterTest as shared
DATA=json.loads(Path(os.environ.get('PG_CHARACTER_BATCH_MANIFEST',ROOT/'Tools/Art/ToonCharacters/manifest.json')).read_text(encoding='utf-8'))
DEST=DATA['destination']
OUT=Path(os.environ.get('PG_CHARACTER_BATCH_OUTPUT',ROOT/'Saved/ToonCharacters'))
OUT.mkdir(parents=True,exist_ok=True)
LIB=unreal.MaterialEditingLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
EAL=unreal.EditorAssetLibrary
PROFILES=json.loads((ROOT/'Tools/Art/ToonTest/shading_profiles.json').read_text())['profiles']
REPORT=dict(status='RUNNING',characters=[],textures={},source_hashes=dict(DATA['source_hashes']))


def canonical(name):
    return re.sub(r'[^a-z0-9]','',re.sub(r'[._]\d{3}$','',name).lower())


def profile(name):
    n=name.lower()
    if any(s in n for s in ('eye','faceparts','facial','face_opas')): return 'detail'
    if 'hair' in n: return 'hair'
    if 'face' in n: return 'face'
    if 'body' in n or 'skin' in n: return 'skin'
    if 'metal' in n or 'lens' in n or 'glass' in n: return 'metal'
    return 'cloth'


def values(name):
    p=PROFILES[name]
    out=values(p['inherits']) if 'inherits' in p else dict(scalars={},vectors={})
    for key in out: out[key].update(p.get(key,{}))
    return out


def texture(path,folder,linear=False):
    key=path+('_linear' if linear else '')
    if key in REPORT['textures']: return unreal.load_asset(REPORT['textures'][key])
    name='T_'+re.sub(r'[^A-Za-z0-9_]','_',Path(path).stem)+'_'+hashlib.sha1(key.encode()).hexdigest()[:8]
    target=folder+'/Textures/'+name
    asset=unreal.load_asset(target) if EAL.does_asset_exist(target) else None
    REPORT['source_hashes'][path]=hashlib.sha256(Path(path).read_bytes()).hexdigest()
    if not asset:
        task=unreal.AssetImportTask()
        task.filename,task.destination_path,task.destination_name=path,folder+'/Textures',name
        task.automated,task.save=True,True
        TOOLS.import_asset_tasks([task])
        asset=unreal.load_asset(target)
    assert isinstance(asset,unreal.Texture2D),path
    asset.set_editor_property('srgb',not linear)
    asset.set_editor_property('max_texture_size',2048)
    shared.save(asset)
    REPORT['textures'][key]=target
    return asset


def resolve(spec,slot):
    bindings={canonical(k):v for k,v in spec['bindings'].items()}
    guid=bindings.get(canonical(slot))
    if guid:
        assert guid in spec['materials'],('Missing source material',spec['name'],slot,guid)
        return guid,spec['materials'][guid]
    candidates=[(g,m) for g,m in spec['materials'].items() if canonical(m['name'])==canonical(slot)]
    assert len(candidates)==1,('Unresolved or ambiguous slot',spec['name'],slot,[m['name'] for _,m in candidates])
    return candidates[0]


def character(spec):
    name=spec['name']
    folder=DEST+'/'+name
    shared.source_root=Path(spec['fbx']).parent
    shared.destination=folder
    shared.mesh_path=folder+'/SK_PG_'+name
    shared.manifest=dict(fbx=Path(spec['fbx']).name,skeletal_mesh='SK_PG_'+name)
    reimport=name in os.environ.get('PG_CHARACTER_BATCH_REIMPORT','').split(',')
    if reimport:
        # UE 5.8 otherwise reroutes replacement through Interchange, ignoring
        # the legacy FbxImportUI options used by this pipeline's initial import.
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
    mesh=unreal.load_asset(shared.mesh_path) if EAL.does_asset_exist(shared.mesh_path) and not reimport else shared.import_mesh()
    row=dict(name=name,mesh=mesh.get_path_name(),slots=[],status='RUNNING')
    REPORT['characters'].append(row)
    slots=list(mesh.get_editor_property('materials'))
    for i,slot in enumerate(slots):
        slot_name=str(slot.material_slot_name)
        guid,source=resolve(spec,slot_name)
        assert source['shader_resolved'],source['source_material']
        # These Suiha texture GUIDs are absent from the original project's Assets.
        # Preserve metal/lens tint and lens opacity.
        missing_source=(name=='Suiha' and source['texture_guid'] in
                        ('bfc4d5931015d094bb8fd8ef2f023a16','e48975e0e74a2eb46bdcb57746dc8fc8'))
        assert source['texture'] or not source['texture_guid'] or missing_source,source
        if missing_source:
            row.setdefault('warnings',[]).append(dict(slot=slot_name,missing_texture_guid=source['texture_guid'],
                fallback='White texture with original Unity tint/opacity and PG metal highlight'))
        assert source['opacity_texture'] or source['alpha_mode']==0 or source.get('opacity_texture_default')=='white',source
        p=source.get('profile') or profile(source['name'])
        mi_name='MI_PG_'+name+'_'+re.sub(r'[^A-Za-z0-9_]','_',slot_name)
        target=folder+'/Materials/'+mi_name
        mi=unreal.load_asset(target) if EAL.does_asset_exist(target) else TOOLS.create_asset(mi_name,folder+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        # Unity writes depth for these transparent hair shells. UE translucency
        # cannot reproduce that ordering: use alpha cutout for a stable silhouette.
        depth_hair=(name in ('Suiha','Hichi') or source.get('depth_write',False)) and p=='hair' and source['transparent']
        parent=DATA['masters']['transparent' if source['transparent'] and not depth_hair else 'opaque']
        LIB.set_material_instance_parent(mi,unreal.load_asset(parent))
        tex=texture(source['texture'],folder) if source['texture'] else unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture')
        LIB.set_material_instance_texture_parameter_value(mi,'BaseTexture',tex)
        LIB.set_material_instance_vector_parameter_value(mi,'BaseTint',unreal.LinearColor(*source['tint']))
        if source['opacity_texture']: LIB.set_material_instance_texture_parameter_value(mi,'OpacityTexture',texture(source['opacity_texture'],folder,True))
        elif source.get('opacity_texture_default')=='white': LIB.set_material_instance_texture_parameter_value(mi,'OpacityTexture',unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
        v=values(p)
        face=p in ('face','detail')
        v['scalars'].update(dict(OpacityCutoff=source['cutoff'],MainOpacity=source['main_opacity'],UseBaseAlpha=source['use_base_alpha'],
            AlphaMaskMode=source['alpha_mode'],AlphaMaskScale=source['alpha_scale'],AlphaMaskValue=source['alpha_value'],
            FaceShading=.85 if face else 0.,HairAnisotropy=.8 if p=='hair' else 0.,ShadowCast=0. if face else 1.,
            WorldLightingInfluence=.55 if face else .88,ShadeStrength=.45 if face else .6))
        if depth_hair:
            v['scalars']['OpacityCutoff']=.25
        if p=='hair':v['scalars']['WorldLightingInfluence']=shared.hair_world_lighting_influence()
        for k,value in v['scalars'].items(): LIB.set_material_instance_scalar_parameter_value(mi,k,value)
        for k,value in v['vectors'].items(): LIB.set_material_instance_vector_parameter_value(mi,k,unreal.LinearColor(*value))
        LIB.update_material_instance(mi)
        shared.save(mi)
        slot.material_interface=mi
        slots[i]=slot
        row['slots'].append(dict(slot=slot_name,guid=guid,material=target,parent=parent,base_texture=tex.get_path_name(),profile=p,source=source,
                                depth_writing_hair_cutout=depth_hair))
    mesh.set_editor_property('materials',slots)
    shared.save(mesh)
    row.update(status='PASS',skeleton=mesh.get_editor_property('skeleton').get_path_name())
    (OUT/'configure.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')


def main():
    folder=ROOT/'Content'/DEST.removeprefix('/Game/')
    if folder.exists(): shutil.copytree(folder,OUT/'Backups'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
    for spec in DATA['characters']:
        try: character(spec)
        except Exception:
            REPORT.setdefault('errors',[]).append(dict(name=spec['name'],error=traceback.format_exc()))
            unreal.log_error(REPORT['errors'][-1]['error'])
    assert not REPORT.get('errors'),REPORT.get('errors')
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in REPORT['source_hashes'].items())
    REPORT.update(status='PASS',sources_unchanged=True)


if __name__=='__main__':
    try: main()
    except Exception: REPORT.update(status='FAIL',error=traceback.format_exc())
    finally: (OUT/'configure.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    if REPORT['status']!='PASS': raise RuntimeError(REPORT['error'])

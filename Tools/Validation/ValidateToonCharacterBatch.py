"""Fresh-process validation of saved meshes, toon slots, Blueprints and gallery."""
import hashlib
import json
import os
import math
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=Path(os.environ.get('PG_CHARACTER_BATCH_OUTPUT',ROOT/'Saved/ToonCharacters'))
DATA=json.loads(Path(os.environ.get('PG_CHARACTER_BATCH_MANIFEST',ROOT/'Tools/Art/ToonCharacters/manifest.json')).read_text(encoding='utf-8'))
AUDIT=json.loads((OUT/'configure.json').read_text(encoding='utf-8'))
PREVIEW=json.loads((OUT/'preview.json').read_text(encoding='utf-8'))
REPORT=dict(status='RUNNING',characters=[],slots=0,textures=len(AUDIT['textures']))


def main():
    assert AUDIT['status']=='PASS' and PREVIEW['status']=='CAPTURED'
    assert len(AUDIT['characters'])==len(DATA['characters'])
    lib=unreal.MaterialEditingLibrary
    for row in AUDIT['characters']:
        mesh=unreal.load_asset(row['mesh'])
        assert isinstance(mesh,unreal.SkeletalMesh),row['name']
        assert mesh.get_editor_property('skeleton').get_path_name()==row['skeleton']
        slots=mesh.get_editor_property('materials')
        assert len(slots)==len(row['slots'])
        for slot,entry in zip(slots,row['slots']):
            assert str(slot.material_slot_name)==entry['slot']
            mi=slot.material_interface
            assert mi.get_path_name().split('.')[0]==entry['material']
            assert mi.get_editor_property('parent').get_path_name().split('.')[0]==entry['parent']
            tex=lib.get_material_instance_texture_parameter_value(mi,'BaseTexture')
            assert tex and tex.get_path_name()==entry['base_texture']
            for p,key in [('MainOpacity','main_opacity'),('UseBaseAlpha','use_base_alpha'),('AlphaMaskMode','alpha_mode')]:
                assert abs(lib.get_material_instance_scalar_parameter_value(mi,p)-entry['source'][key])<1e-4
            if entry['source']['opacity_texture'] or entry['source'].get('opacity_texture_default'):
                assert lib.get_material_instance_texture_parameter_value(mi,'OpacityTexture')
            if entry.get('depth_writing_hair_cutout'):
                assert entry['parent'].endswith('M_PGToonWorld_multi')
                assert abs(lib.get_material_instance_scalar_parameter_value(mi,'OpacityCutoff')-.25)<1e-4
            if entry.get('depth_writing_hair_cutout'):
                assert entry['parent'].endswith('M_PGToonWorld_multi')
                assert abs(lib.get_material_instance_scalar_parameter_value(mi,'OpacityCutoff')-.25)<1e-4
            REPORT['slots']+=1
    world=unreal.EditorLoadingAndSavingUtils.load_map(PREVIEW['map'])
    assert world
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    saved=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PGToonPreviewActor)]
    assert len(saved)==len(DATA['characters']),len(saved)
    pp=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.PostProcessVolume))
    assert pp.get_editor_property('unbound')
    for name,spec in PREVIEW['blueprints'].items():
        bp=unreal.load_asset(spec['asset'])
        assert bp and bp.generated_class()
        actor=actors.spawn_actor_from_class(bp.generated_class(),unreal.Vector(0,0,300))
        comp=actor.skeletal_mesh_component
        assert comp.get_skeletal_mesh_asset().get_path_name()==spec['mesh']
        assert comp.does_socket_exist(spec['head_bone'])
        if name=='Yura' and DATA.get('gallery_name')=='L_PG_PlayerModels':
            assert comp.does_socket_exist('PG_HairRoot')
            assert str(comp.get_parent_bone('PG_HairRoot'))=='Head'
        assert comp.get_editor_property('render_custom_depth')
        assert comp.get_editor_property('custom_depth_stencil_value')==73
        origin,extent=actor.get_actor_bounds(False)
        assert all(math.isfinite(v) for v in (origin.x,origin.y,origin.z,extent.x,extent.y,extent.z))
        assert 40<extent.z<160,(name,extent)
        gallery=next(a for a in saved if a.get_actor_label()=='PG Toon '+name)
        assert gallery.toon_presentation.key_light
        # Exercise real MID creation and head/light synchronization on the saved fixture.
        gallery.toon_presentation.initialize(gallery.skeletal_mesh_component)
        assert gallery.toon_presentation.get_dynamic_material_count()==comp.get_num_materials()
        REPORT['characters'].append(dict(name=name,height_cm=extent.z*2,bones=comp.get_num_bones(),slots=comp.get_num_materials(),blueprint=spec['asset']))
        actors.destroy_actor(actor)
    assert len(PREVIEW['images'])==1+3*len(DATA['characters'])
    for image in PREVIEW['images']: assert Path(image).is_file() and Path(image).stat().st_size>10000
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in AUDIT['source_hashes'].items())
    REPORT.update(status='PASS',sources_unchanged=True,map=PREVIEW['map'],images=len(PREVIEW['images']))


try: main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally: (OUT/'validate.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
if REPORT['status']!='PASS': raise RuntimeError(REPORT['error'])

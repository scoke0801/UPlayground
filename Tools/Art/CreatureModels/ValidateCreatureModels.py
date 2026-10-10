"""Fresh-process checks for saved meshes, variants, animation timing and bone poses."""
import hashlib,json,math,traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();ART=ROOT/'Tools/Art/CreatureModels';OUT=ROOT/'Saved/CreatureModels'
DATA=json.loads((ART/'manifest.json').read_text(encoding='utf-8'))
REPORT=dict(status='RUNNING',models=[])
def main():
    cfg=json.loads((OUT/'configure.json').read_text());assert cfg['status']=='PASS'
    for f in DATA['files']:
        assert hashlib.sha256((ART/f['path']).read_bytes()).hexdigest()==f['sha256'],f['path']
    for spec,row in zip(DATA['models'],cfg['models']):
        mesh=unreal.load_asset(row['mesh']);assert isinstance(mesh,unreal.SkeletalMesh)
        skeleton=mesh.get_editor_property('skeleton');assert skeleton.get_path_name()==row['skeleton']
        assert mesh.get_editor_property('physics_asset')
        assert all(s.material_interface for s in mesh.get_editor_property('materials'))
        bounds=mesh.get_bounds();assert 5<bounds.sphere_radius<10000
        comp=unreal.SkeletalMeshComponent();comp.set_skeletal_mesh_asset(mesh)
        bones=[comp.get_bone_name(i) for i in range(comp.get_num_bones())];assert len(bones)>3
        clips=[]
        for c in spec['clips']:
            a=unreal.load_asset(row['clips'][c['name']]);assert isinstance(a,unreal.AnimSequence)
            assert a.get_editor_property('skeleton')==skeleton
            expected=(c['last']-c['first'])/30.
            assert abs(a.get_play_length()-expected)<.035,(spec['name'],c['name'],a.get_play_length(),expected)
            poses=[]
            for fraction in (0.,.25,.5,.75,1.):
                pose=a.get_anim_pose_at_time(a.get_play_length()*fraction,unreal.AnimPoseEvaluationOptions())
                frame=[]
                for bone in bones:
                    t=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
                    values=[t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,t.scale3d.x,t.scale3d.y,t.scale3d.z]
                    assert all(math.isfinite(v) and abs(v)<100000 for v in values),(c['name'],str(bone),values)
                    frame.extend(values)
                poses.append(frame)
            delta=max(abs(v-ref) for frame in poses[1:] for v,ref in zip(frame,poses[0]))
            assert delta>.0001,(spec['name'],c['name'],'static animation')
            clips.append(dict(name=c['name'],seconds=a.get_play_length(),pose_delta=delta))
        for path in row['montages'].values():
            a=unreal.load_asset(path);assert a.get_editor_property('skeleton')==skeleton
            assert a.get_play_length()>0
        for variant_index,path in enumerate(row['blueprints']):
            bp=unreal.load_asset(path);assert bp and bp.generated_class()
            cdo=unreal.get_default_object(bp.generated_class());comp=cdo.skeletal_mesh_component
            assert comp.get_skeletal_mesh_asset()==mesh
            play=comp.get_editor_property('animation_data');assert play.get_editor_property('anim_to_play').get_editor_property('skeleton')==skeleton
            assert all(comp.get_material(i)==unreal.load_asset(cfg['materials'][spec['materials'][variant_index]]) for i in range(len(mesh.get_editor_property('materials'))))
        if row['blend']:
            blend=unreal.load_asset(row['blend'])
            for speed in (0,75,150,225,300):assert unreal.PGHumanoidLocomotionTools.get_blend_sample_count(blend,unreal.Vector(speed,0,0))>0
        REPORT['models'].append(dict(name=spec['name'],bones=len(bones),clips=clips,variants=len(row['blueprints'])))
    lib=unreal.MaterialEditingLibrary
    master=unreal.load_asset(DATA['destination']+'/Materials/M_PG_CreaturePBR')
    assert master.get_editor_property('used_with_skeletal_mesh')
    rough=lib.get_material_property_input_node(master,unreal.MaterialProperty.MP_ROUGHNESS)
    assert isinstance(rough,unreal.MaterialExpressionOneMinus)
    assert len(lib.get_inputs_for_material_expression(master,rough))==1
    for material in DATA['materials']:
        mi=unreal.load_asset(cfg['materials'][material['name']])
        for parameter,source in [('BaseColor','_BaseMap'),('Normal','_BumpMap'),('MetallicSmoothness','_MetallicGlossMap'),('Emission','_EmissionMap')]:
            if material['textures'].get(source):
                t=lib.get_material_instance_texture_parameter_value(mi,parameter)
                assert t and t.get_name()==Path(material['textures'][source]).stem,(material['name'],parameter)
    for key,path in cfg['textures'].items():
        texture=unreal.load_asset(path);assert texture
        if key.endswith(':normal'):assert texture.get_editor_property('flip_green_channel') and not texture.get_editor_property('srgb')
        if key.endswith(':mask'):assert not texture.get_editor_property('srgb')
    REPORT.update(status='PASS',sources_unchanged=True,total_clips=sum(len(m['clips']) for m in REPORT['models']))
try:main()
except Exception:REPORT.update(status='FAIL',error=traceback.format_exc());unreal.log_error(REPORT['error'])
finally:(OUT/'validate.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
if REPORT['status']!='PASS':raise RuntimeError(REPORT['error'])

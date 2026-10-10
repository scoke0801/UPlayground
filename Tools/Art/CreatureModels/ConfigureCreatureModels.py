"""Build PBR materials, placement Blueprints, montages and locomotion assets."""
import json, re, sys, traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();ART=ROOT/'Tools/Art/CreatureModels'
DATA=json.loads((ART/'manifest.json').read_text(encoding='utf-8'))
DEST=DATA['destination'];OUT=ROOT/'Saved/CreatureModels'
EAL=unreal.EditorAssetLibrary;TOOLS=unreal.AssetToolsHelpers.get_asset_tools();LIB=unreal.MaterialEditingLibrary
REPORT=dict(status='RUNNING',textures={},materials={},models=[])
def save(a):assert EAL.save_loaded_asset(a,only_if_is_dirty=False),a.get_path_name()
def clean(n):return re.sub('[^A-Za-z0-9_]+','_',n).strip('_')
def own(name,folder,cls,factory):
    p=folder+'/'+name
    return unreal.load_asset(p) if EAL.does_asset_exist(p) else TOOLS.create_asset(name,folder,cls,factory)
def tex(file,kind):
    key=file+':'+kind
    if key in REPORT['textures']:return unreal.load_asset(REPORT['textures'][key])
    folder=DEST+'/Textures';name=Path(file).stem;p=folder+'/'+name
    t=unreal.load_asset(p) if EAL.does_asset_exist(p) else None
    if not t:
        task=unreal.AssetImportTask();task.filename=str(ART/file);task.destination_path=folder;task.destination_name=name
        task.automated=True;task.save=True;TOOLS.import_asset_tasks([task]);t=unreal.load_asset(p)
    assert t,file
    t.set_editor_property('srgb',kind=='color')
    t.set_editor_property('max_texture_size',2048)
    if kind=='normal':
        t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property('flip_green_channel',True)
    elif kind=='mask':t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    save(t);REPORT['textures'][key]=p
    return t
def master():
    mat=own('M_PG_CreaturePBR',DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_skeletal_mesh',True)
    LIB.delete_all_material_expressions(mat)
    def node(cls,x,y):return LIB.create_material_expression(mat,cls,x,y)
    def sample(name,texture,y,normal=False,linear=False):
        n=node(unreal.MaterialExpressionTextureSampleParameter2D,-700,y)
        n.set_editor_property('parameter_name',name);n.set_editor_property('texture',texture)
        n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if linear else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        return n
    first=DATA['materials'][0]
    base=sample('BaseColor',tex(first['textures']['_BaseMap'],'color'),0)
    normal=sample('Normal',tex(first['textures']['_BumpMap'],'normal'),220,normal=True)
    mask=sample('MetallicSmoothness',tex(first['textures']['_MetallicGlossMap'],'mask'),440,linear=True)
    LIB.connect_material_property(base,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(normal,'RGB',unreal.MaterialProperty.MP_NORMAL)
    LIB.connect_material_property(mask,'R',unreal.MaterialProperty.MP_METALLIC)
    smooth=node(unreal.MaterialExpressionScalarParameter,-650,660);smooth.set_editor_property('parameter_name','Smoothness');smooth.set_editor_property('default_value',.5)
    mul=node(unreal.MaterialExpressionMultiply,-360,500)
    LIB.connect_material_expressions(mask,'A',mul,'A');LIB.connect_material_expressions(smooth,'',mul,'B')
    inv=node(unreal.MaterialExpressionOneMinus,-170,500);assert LIB.connect_material_expressions(mul,'',inv,'')
    LIB.connect_material_property(inv,'',unreal.MaterialProperty.MP_ROUGHNESS)
    e=next(m for m in DATA['materials'] if m['textures'].get('_EmissionMap'))
    emission=sample('Emission',tex(e['textures']['_EmissionMap'],'color'),850)
    tint=node(unreal.MaterialExpressionVectorParameter,-640,1080);tint.set_editor_property('parameter_name','EmissionColor');tint.set_editor_property('default_value',unreal.LinearColor(0,0,0,1))
    emul=node(unreal.MaterialExpressionMultiply,-320,880)
    LIB.connect_material_expressions(emission,'RGB',emul,'A');LIB.connect_material_expressions(tint,'RGB',emul,'B')
    LIB.connect_material_property(emul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat);save(mat);return mat
def material(m,parent):
    mi=own('MI_PG_'+m['name'].replace('_Material',''),DEST+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(mi,parent)
    for param,src,kind in [('BaseColor','_BaseMap','color'),('Normal','_BumpMap','normal'),('MetallicSmoothness','_MetallicGlossMap','mask'),('Emission','_EmissionMap','color')]:
        if m['textures'].get(src):LIB.set_material_instance_texture_parameter_value(mi,param,tex(m['textures'][src],kind))
    LIB.set_material_instance_scalar_parameter_value(mi,'Smoothness',m['floats'].get('_Smoothness',.5))
    LIB.set_material_instance_vector_parameter_value(mi,'EmissionColor',unreal.LinearColor(*m['colors'].get('_EmissionColor',[0,0,0,1])))
    LIB.update_material_instance(mi);save(mi);REPORT['materials'][m['name']]=mi.get_path_name();return mi
def main():
    parent=master();materials={m['name']:material(m,parent) for m in DATA['materials']}
    for m in DATA['models']:
        name=m['name'];folder=DEST+'/'+name;mesh=unreal.load_asset(folder+'/SK_PG_'+name);assert mesh
        sk=mesh.get_editor_property('skeleton')
        slots=list(mesh.get_editor_property('materials'))
        assert len(slots)==1,(name,'Unexpected material layout')
        for s in slots:s.material_interface=materials[m['materials'][0]]
        mesh.set_editor_property('materials',slots);save(mesh)
        assets=[unreal.load_asset(p) for p in EAL.list_assets(folder+'/Animations')]
        clips={};montages={}
        for c in m['clips']:
            target=folder+'/Animations/AS_PG_'+name+'_'+clean(c['name'])
            a=unreal.load_asset(target) if EAL.does_asset_exist(target) else None
            if not a:
                # Importer sanitizes pipes and spaces independently.
                canonical=lambda s:re.sub('[^a-z0-9]','',s.lower())
                candidates=[a for a in assets if isinstance(a,unreal.AnimSequence) and canonical(a.get_name())==canonical('AS_PG_'+name+'_'+c['name'])]
                assert len(candidates)==1,(name,c,candidates,[a.get_name() for a in assets])
                a=candidates[0]
                if a.get_path_name().split('.')[0]!=target:
                    assert EAL.rename_asset(a.get_path_name(),target)
            a.set_editor_property('enable_root_motion',False)
            # Preserve original vertical motion in landing/takeoff and non-zero flight takes.
            a.set_editor_property('force_root_lock',False)
            save(a);clips[c['name']]=a.get_path_name()
            if not c['loop']:
                factory=unreal.AnimMontageFactory();factory.set_editor_property('target_skeleton',sk);factory.set_editor_property('source_animation',a)
                am=own('AM_PG_'+name+'_'+clean(c['name']),folder+'/Montages',unreal.AnimMontage,factory)
                save(am);montages[c['name']]=am.get_path_name()
        blend=None
        if 'Walk' in clips:
            factory=unreal.BlendSpaceFactory1D();factory.set_editor_property('target_skeleton',sk)
            blend=own('BS_PG_'+name+'_Ground',folder,unreal.BlendSpace1D,factory)
            axis=unreal.BlendParameter();axis.set_editor_property('display_name','Speed');axis.set_editor_property('min',0.);axis.set_editor_property('max',600. if name=='Griffin' else 300.)
            blend.set_editor_property('blend_parameters',[axis,axis,axis]);samples=[]
            for value,clip in [(0,'Idle'),(axis.get_editor_property('max')*.5,'Walk'),(axis.get_editor_property('max'),'Run')]:
                s=unreal.BlendSample();s.set_editor_property('animation',unreal.load_asset(clips[clip]));s.set_editor_property('sample_value',unreal.Vector(value,0,0));samples.append(s)
            blend.set_editor_property('sample_data',samples);unreal.PGHumanoidLocomotionTools.rebuild_blend_space(blend);save(blend)
        bps=[];idle=unreal.load_asset(clips['Idle_Battle' if name=='MainPlant' else 'Idle'])
        for material_name in m['materials']:
            suffix=material_name.replace('_Material','').replace('Griffin_','').replace('Main_Plant_','').replace('Enemy_Plant_','')
            factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.SkeletalMeshActor)
            bp=own('BP_PG_'+name+'_'+suffix,folder+'/Blueprints',unreal.Blueprint,factory)
            cdo=unreal.get_default_object(bp.generated_class());comp=cdo.skeletal_mesh_component
            comp.set_skeletal_mesh_asset(mesh);comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            for i in range(len(slots)):comp.set_material(i,materials[material_name])
            comp.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            play=comp.get_editor_property('animation_data');play.set_editor_property('anim_to_play',idle);play.set_editor_property('saved_looping',True);play.set_editor_property('saved_playing',True)
            comp.set_editor_property('animation_data',play)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp);bps.append(bp.get_path_name())
        REPORT['models'].append(dict(name=name,mesh=mesh.get_path_name(),skeleton=sk.get_path_name(),clips=clips,montages=montages,blueprints=bps,blend=blend.get_path_name() if blend else None))
        (OUT/'configure.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    REPORT['status']='PASS'
try:main()
except Exception:REPORT.update(status='FAIL',error=traceback.format_exc());unreal.log_error(REPORT['error'])
finally:(OUT/'configure.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')

if REPORT['status']!='PASS':raise RuntimeError(REPORT['error'])

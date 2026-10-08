"""Isolate soft-shadow projection artifacts without saving any scene/material assets."""
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
RUN = Path(os.environ['PG_HAIR_QUALITY_RUN'])
DATA = json.loads((ROOT/'Saved/BokuseiShadingComparison/configure.json').read_text(encoding='utf-8'))
SETTINGS = json.loads((ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison/hair_shadow_settings.json').read_text(encoding='utf-8'))
REPORT = dict(status='RUNNING',run=str(RUN),images=[],cases=[])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.EditorLoadingAndSavingUtils.load_map(DATA['map'])
models = sorted([a for a in actors.get_all_level_actors() if isinstance(a,unreal.SkeletalMeshActor)],key=lambda a:a.get_actor_location().x)
key = models[1].toon_presentation.key_light
key_rotation = key.get_actor_rotation()
shadow_master = unreal.load_asset('/Game/Art/ToonTest/BokuseiShadingComparison/Materials/M_PGComparison_HairShadow')
final_probe = '-PGHairQualityFinal' in unreal.SystemLibrary.get_command_line()
temporal_probe = '-PGHairQualityTemporal' in unreal.SystemLibrary.get_command_line()
if final_probe:
    assert DATA['hair_shadow']['settings'] == SETTINGS, 'Settings not applied to the saved map'
    assert shadow_master.get_editor_property('two_sided') == SETTINGS['two_sided']
    assert abs(key.light_component.get_editor_property('light_source_angle')-SETTINGS['light_source_angle_degrees']) < 1e-6
    for model in [models[5], models[7]]:
        for row in DATA['hair_shadow']['slots']:
            material=model.hair_shadow_proxy.get_material(row['index'])
            for name,setting in [('OpacityCutoff','opacity_cutoff'),('ShadowInset','inset_cm')]:
                assert abs(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,name)-SETTINGS[setting]) < 1e-6
    REPORT['saved_settings_reload'] = dict(status='PASS',settings=SETTINGS)
if '-PGHairQualityDepthBias' in unreal.SystemLibrary.get_command_line():
    lib=unreal.MaterialEditingLibrary
    def expression(cls):return lib.create_material_expression(shadow_master,cls)
    normal=expression(unreal.MaterialExpressionVertexNormalWS)
    direction=expression(unreal.MaterialExpressionVectorParameter);direction.set_editor_property('parameter_name','LightDirection')
    bias=expression(unreal.MaterialExpressionScalarParameter);bias.set_editor_property('parameter_name','ShadowDepthBias');bias.set_editor_property('default_value',0)
    inset=expression(unreal.MaterialExpressionScalarParameter);inset.set_editor_property('parameter_name','ShadowInset');inset.set_editor_property('default_value',-.08)
    wpo=expression(unreal.MaterialExpressionCustom)
    wpo.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    wpo.set_editor_property('code','return N * Inset + normalize(Direction) * max(Bias, 0.0);')
    inputs=[]
    for name in ['N','Direction','Bias','Inset']:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);inputs.append(pin)
    wpo.set_editor_property('inputs',inputs)
    for node,pin,name in [(normal,'','N'),(direction,'RGB','Direction'),(bias,'','Bias'),(inset,'','Inset')]:
        assert lib.connect_material_expressions(node,pin,wpo,name)
    assert lib.connect_material_property(wpo,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    lib.recompile_material(shadow_master)
idle = unreal.load_asset('/Game/Art/ToonTest/Bokusei/Animation/PGBokusei_AS_Anime_KC_Idle')
models[0].skeletal_mesh_component.override_animation_data(idle,True,False,1.25,0)
for model in models[1:]:
    model.toon_presentation.initialize(model.skeletal_mesh_component)
    proxy = model.hair_shadow_proxy
    if proxy.get_skeletal_mesh_asset():
        for i in [4,5]: proxy.create_dynamic_material_instance(i)
for actor in actors.get_all_level_actors():
    if actor.actor_has_tag('PGShadingShadowCaster'):actor.static_mesh_component.set_cast_shadow(False)
performance = unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
CVARS = ['r.Shadow.Virtual.SMRT.RayCountDirectional','r.Shadow.Virtual.SMRT.SamplesPerRayDirectional',
         'r.Shadow.Virtual.SMRT.AdaptiveRayCount','r.Shadow.Virtual.SMRT.ExtrapolateMaxSlopeDirectional',
         'r.Shadow.Virtual.ResolutionLodBiasDirectional','r.Shadow.Virtual.NormalBias',
         'r.Shadow.Virtual.ResolutionLodBiasDirectionalMoving','r.Shadow.Virtual.Enable',
         'r.Shadow.Virtual.Cache','r.Lumen.DiffuseIndirect.Allow','r.AmbientOcclusionLevels']
defaults = {name:unreal.SystemLibrary.get_console_variable_float_value(name) for name in CVARS}
REPORT['defaults'] = defaults
REPORT['source_angle'] = key.light_component.get_editor_property('light_source_angle')
REPORT['anti_aliasing'] = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
REPORT['vsm_enabled'] = unreal.SystemLibrary.get_console_variable_int_value('r.Shadow.Virtual.Enable')
REPORT['translucent_quality'] = unreal.SystemLibrary.get_console_variable_int_value('r.Shadow.Virtual.TranslucentQuality')
REPORT['key_properties'] = {n:str(key.light_component.get_editor_property(n)) for n in
                           ['cast_shadows','cast_dynamic_shadows','contact_shadow_length','shadow_bias','shadow_slope_bias',
                            'forward_shading_priority','mobility']}
for command in ['DisableAllScreenMessages','r.ScreenPercentage 100','r.Streaming.FullyLoadUsedTextures 1','viewmode lit','t.MaxFPS 60']:
    unreal.SystemLibrary.execute_console_command(world,command)
level.editor_set_game_view(True)

cases = [dict(name='Warmup',source_angle=3),dict(name='Original',source_angle=3),
         dict(name='SmallSource',source_angle=.535),dict(name='HardSource',source_angle=0),
         dict(name='HighSamples',source_angle=3,cvars={CVARS[0]:32,CVARS[1]:8,CVARS[2]:0}),
         dict(name='NoExtrapolation',source_angle=3,cvars={CVARS[3]:0}),
         dict(name='NoInset',source_angle=3,inset=0),dict(name='HairOff',source_angle=3,hair=False)]
if '-PGHairQualityProjection' in unreal.SystemLibrary.get_command_line():
    cases = [dict(name='Warmup',source_angle=3),dict(name='Original',source_angle=3),
             dict(name='ZeroNormalBias',source_angle=3,cvars={CVARS[5]:0}),
             dict(name='NormalBiasOne',source_angle=3,cvars={CVARS[5]:1}),
             dict(name='NormalBiasThree',source_angle=3,cvars={CVARS[5]:3}),
             dict(name='HigherResolution',source_angle=3,cvars={CVARS[4]:-4,CVARS[6]:-4}),
             dict(name='HigherAlphaCutoff',source_angle=3,cutoff=.6),dict(name='LargeSource',source_angle=6)]
if '-PGHairQualityFinal' in unreal.SystemLibrary.get_command_line():
    selected = key.light_component.get_editor_property('light_source_angle')
    cases = [dict(name='Warmup',source_angle=selected)]
    for stage in [5,7]:
        for view in ['Front','Quarter']:
            for enabled in [True,False]:
                cases.append(dict(name=f'Stage{stage+1}{view}Hair'+('On' if enabled else 'Off'),source_angle=selected,
                                  stage=stage,view=view,hair=enabled))
        for angle in [-60,0,60]:
                cases.append(dict(name=f'Stage{stage+1}Light{angle:+04d}',source_angle=selected,stage=stage,angle=angle))
    for case in cases:
        case.update(cutoff=SETTINGS['opacity_cutoff'], inset=SETTINGS['inset_cm'], two_sided=SETTINGS['two_sided'])
if '-PGHairQualityPath' in unreal.SystemLibrary.get_command_line():
    cases = [dict(name='Warmup',source_angle=3),dict(name='Original',source_angle=3),
             dict(name='KeyShadowOff',source_angle=3,key_shadow=False),
             dict(name='IndirectOff',source_angle=3,cvars={CVARS[9]:0,CVARS[10]:0}),
             dict(name='CacheOff',source_angle=3,cvars={CVARS[8]:0}),
             dict(name='ExtremeSource',source_angle=30),
             dict(name='LegacyShadow',source_angle=3,cvars={CVARS[7]:0}),
             dict(name='LegacyHighBias',source_angle=3,cvars={CVARS[7]:0},bias=2),
             dict(name='HairOff',source_angle=3,hair=False)]
if '-PGHairQualityGeometry' in unreal.SystemLibrary.get_command_line():
    cases = [dict(name='Warmup',source_angle=3),dict(name='Original',source_angle=3),
             dict(name='MainHairOnly',source_angle=3,slots=[4]),dict(name='AlphaHairOnly',source_angle=3,slots=[5]),
             dict(name='FilledAlpha',source_angle=3,cutoff=.05),dict(name='InsetHalfCm',source_angle=3,inset=-.5),
             dict(name='NoSoftSampling',source_angle=0,cvars={CVARS[0]:0}),dict(name='HairOff',source_angle=3,hair=False)]
if '-PGHairQualityCulling' in unreal.SystemLibrary.get_command_line():
    cases = [dict(name='Warmup',source_angle=3),dict(name='Original',source_angle=3),
             dict(name='OneSided',source_angle=3,two_sided=False),
             dict(name='OneSidedDense',source_angle=3,two_sided=False,cutoff=.9),
             dict(name='OneSidedSolid',source_angle=3,two_sided=False,solid=True),
             dict(name='TwoSidedSolid',source_angle=3,solid=True),dict(name='HairOff',source_angle=3,hair=False)]
if '-PGHairQualityCandidate' in unreal.SystemLibrary.get_command_line():
    candidate=dict(source_angle=SETTINGS['light_source_angle_degrees'],cutoff=SETTINGS['opacity_cutoff'],
                   inset=SETTINGS['inset_cm'],two_sided=SETTINGS['two_sided'])
    cases=[dict(name='Warmup',source_angle=3)]
    for stage in [5,7]:
        for view in ['Front','Quarter']:
            cases.append(dict(name=f'BeforeStage{stage+1}{view}',source_angle=3,stage=stage,view=view))
            cases.append(dict(candidate,name=f'CandidateStage{stage+1}{view}',stage=stage,view=view))
            cases.append(dict(candidate,name=f'CandidateStage{stage+1}{view}Off',stage=stage,view=view,hair=False))
    for angle in [-60,0,60]:
        cases.append(dict(candidate,name=f'CandidateLight{angle:+04d}',stage=7,angle=angle))
if '-PGHairQualityTranslucentHQ' in unreal.SystemLibrary.get_command_line():
    assert REPORT['translucent_quality']==1,REPORT['translucent_quality']
    cases=[dict(name='Warmup',source_angle=3),dict(name='OriginalHQFront',source_angle=3),
           dict(name='OriginalHQQuarter',source_angle=3,view='Quarter'),
           dict(name='OriginalHQHairOff',source_angle=3,hair=False)]
if '-PGHairQualityLayers' in unreal.SystemLibrary.get_command_line():
    cases=[dict(name='Warmup',source_angle=3),dict(name='Original',source_angle=3),
           dict(name='VisibleHairOff',source_angle=3,visible_hair=False),
           dict(name='FaceLightingOff',source_angle=3,face_lighting=False)]
if '-PGHairQualityDepthBias' in unreal.SystemLibrary.get_command_line():
    cases=[dict(name='Warmup',source_angle=3),dict(name='Original',source_angle=3)]
    for bias in [.1,.3,.5,1.,2.]:cases.append(dict(name=f'DepthBias{bias:g}',source_angle=3,inset=0,depth_bias=bias))
    cases.append(dict(name='HairOff',source_angle=3,hair=False))
if '-PGHairQualitySoftness' in unreal.SystemLibrary.get_command_line():
    stable={CVARS[0]:16,CVARS[1]:8,CVARS[2]:0}
    cases=[dict(name='Warmup',source_angle=3,stage=7),dict(name='OriginalSource',source_angle=3,stage=7)]
    for angle in [10,15,20]:
        cases.append(dict(name=f'Source{angle}',source_angle=angle,stage=7))
    for angle in [15,20]:
        cases.append(dict(name=f'Source{angle}Stable',source_angle=angle,stage=7,cvars=stable))
    cases += [dict(name='Source15StableOff',source_angle=15,stage=7,cvars=stable,hair=False),
              dict(name='Source15StableQuarter',source_angle=15,stage=7,cvars=stable,view='Quarter')]
REPORT['protected'] = DATA['protected']
started = last = time.monotonic()
index, prepared, pending = 0,False,None
pie_started, pie_ready, pawn, controller = None,False,None,None
REPORT['capture_method'] = 'PIE Shot with TSR history' if temporal_probe else 'Single-frame HighResShot'

def finish(error=None):
    if not error:
        try:
            for path,expected in DATA['protected'].items():
                assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==expected,path
        except Exception:
            error=traceback.format_exc()
    REPORT['status'] = 'FAIL' if error else 'PASS'
    if error:REPORT['error']=error;unreal.log_error(error)
    (RUN/'quality.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    performance.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    unreal.unregister_slate_post_tick_callback(handle)
    if pie_started:level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def tick(_dt):
    global index,prepared,pending,last,pie_started,pie_ready,world,models,key,pawn,controller
    try:
        now=time.monotonic()
        if now-started>210:finish('Hair quality capture timed out');return
        if now-started<22 or now-last<2.5:return
        if temporal_probe and not pie_ready:
            if not pie_started:
                for model in models[1:]:model.toon_presentation.initialize(None)
                level.editor_request_begin_play()
                pie_started=now;last=now;return
            world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not world:return
            models=sorted(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SkeletalMeshActor),key=lambda a:a.get_actor_location().x)
            assert len(models)==len(DATA['stages'])
            key=models[1].toon_presentation.key_light
            models[0].skeletal_mesh_component.override_animation_data(idle,True,False,1.25,0)
            for model in models[1:]:
                proxy=model.hair_shadow_proxy
                if proxy.get_skeletal_mesh_asset():
                    for i in [4,5]:proxy.create_dynamic_material_instance(i)
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor):
                if actor.actor_has_tag('PGShadingShadowCaster'):actor.static_mesh_component.set_cast_shadow(False)
            controller=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=controller.get_controlled_pawn()
            assert isinstance(pawn,unreal.PGShadingComparisonPawn)
            assert unreal.PGToonPreviewActor.set_preview_viewport_size(world,1600,900)
            pie_ready=True;last=now;return
        if pending:
            if not pending.is_file() or pending.stat().st_size<10000:return
            if cases[index]['name']!='Warmup':
                REPORT['images'].append(str(pending))
                REPORT['cases'].append(cases[index])
            index+=1;pending=None;prepared=False;last=now
            if index==len(cases):finish()
            return
        case=cases[index]
        if not prepared:
            two_sided=case.get('two_sided',True)
            if shadow_master.get_editor_property('two_sided')!=two_sided:
                shadow_master.set_editor_property('two_sided',two_sided)
                unreal.MaterialEditingLibrary.recompile_material(shadow_master)
            for name,value in defaults.items():
                value=case.get('cvars',{}).get(name,value)
                unreal.SystemLibrary.execute_console_command(world,f'{name} {value:g}')
            key.light_component.set_light_source_angle(case['source_angle'])
            key.light_component.set_cast_shadows(case.get('key_shadow',True))
            key.light_component.set_editor_property('shadow_bias',case.get('bias',.5))
            rotation=key_rotation
            if 'angle' in case:
                a=math.radians(case['angle'])
                L=unreal.Vector(math.sin(a)*.94,math.cos(a)*.94,.342)
                rotation=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(),L*-1)
            key.set_actor_rotation(rotation,False)
            for model in models[1:]:
                if '-PGHairQualityLayers' in unreal.SystemLibrary.get_command_line():
                    mesh=model.skeletal_mesh_component
                    for i in [4,5]:mesh.get_material(i).set_scalar_parameter_value('MainOpacity',1 if case.get('visible_hair',True) else 0)
                    if model in [models[5],models[7]]:
                        mesh.get_material(1).set_scalar_parameter_value('WorldLightingInfluence',.65 if case.get('face_lighting',True) else 0)
                proxy=model.hair_shadow_proxy
                if proxy.get_skeletal_mesh_asset():
                    proxy.set_cast_shadow(case.get('hair',True))
                    for i in [4,5]:
                        mid=proxy.get_material(i)
                        mid.set_scalar_parameter_value('ShadowInset',case.get('inset',-.08))
                        mid.set_scalar_parameter_value('OpacityCutoff',case.get('cutoff',.35) if i in case.get('slots',[4,5]) else 2)
                        mid.set_scalar_parameter_value('AlphaMaskMode',0 if case.get('solid') else int(i==5))
                        mid.set_scalar_parameter_value('UseBaseAlpha',0 if case.get('solid') else 1)
                        mid.set_scalar_parameter_value('ShadowDepthBias',case.get('depth_bias',0))
                        light_dir=key.get_actor_forward_vector()
                        mid.set_vector_parameter_value('LightDirection',unreal.LinearColor(light_dir.x,light_dir.y,light_dir.z,0))
                model.toon_presentation.refresh_presentation()
            stage=case.get('stage',5)
            base=DATA['stages'][stage]['position']
            location=unreal.Vector(base+65,115,150) if case.get('view')=='Quarter' else unreal.Vector(base,135,144)
            rotation=unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(base,0,139))
            if temporal_probe:
                pawn.set_actor_location(location,False,True)
                controller.set_control_rotation(rotation)
            else:
                level.set_level_viewport_camera_info(location,rotation,'None')
                level.set_level_viewport_fov(45,'None')
            prepared=True;last=now;return
        pending=RUN/(case['name']+'.png')
        command=f'Shot filename="{pending.as_posix()}" -nosuffix' if temporal_probe else f'HighResShot 1600x900 filename="{pending.as_posix()}"'
        unreal.SystemLibrary.execute_console_command(world,command)
        last=now
    except Exception:finish(traceback.format_exc())

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)

"""PIE temporal captures at output resolution, with no saved map or user settings."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=Path(os.environ['PG_TOON_UPGRADE_RUN'])
NATIVE='-PGNativeToon' in unreal.SystemLibrary.get_command_line()
HAIR='-PGHairComparison' in unreal.SystemLibrary.get_command_line()
IMPROVED='-PGImprovedMap' in unreal.SystemLibrary.get_command_line()
CONTROLS='-PGToonControls' in unreal.SystemLibrary.get_command_line()
SOFT_HAIR=os.environ.get('PG_HAIR_SOFTNESS_PREVIEW') == '1'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Art/ToonTest/Improvement/Maps/L_PGToon_ImprovedComparison' if IMPROVED else '/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison')
models=sorted([a for a in actors.get_all_level_actors() if a.actor_has_tag('PGShadingComparisonModel')],key=lambda a:a.get_actor_location().x)
assert len(models)==8
if SOFT_HAIR:
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag('PGShadingGuest'):
            actor.set_actor_hidden_in_game(True)
if NATIVE:
    native=json.loads((ROOT/'Saved/ToonImprovement/Native/configure.json').read_text(encoding='utf-8'))
    assert native['status']=='PASS' and unreal.SystemLibrary.get_console_variable_int_value('r.Substrate')==1
    for row in native['materials']:models[7].skeletal_mesh_component.set_material(row['index'],unreal.load_asset(row['candidate']))
# Freeze a single pose before PIE. The renderer still accumulates real temporal history.
animation=json.loads((ROOT/'Saved/BokuseiShadingComparison/configure.json').read_text())['animation']
models[0].skeletal_mesh_component.override_animation_data(unreal.load_asset(animation),True,False,1.25,0)
phases=[dict(stage=stage,view=view,angle=angle,screen=screen)
        for stage in ([6,7] if NATIVE else [4,6,7])
        for view in ['face','quarter'] for angle in [0,60,180] for screen in ([50,100] if NATIVE else [50,67,100])]
if HAIR:
    phases=[dict(stage=7,view='face',angle=angle,screen=100,proxy=proxy,source=source)
        for angle in [0,60,120] for source in [3,20] for proxy in ['cards','coarse','none']]
if SOFT_HAIR:
    phases=[dict(stage=stage,view='face',angle=angle,screen=100)
            for stage in [4,7] for angle in [0,60,120]]
    phases += [dict(stage=7,view='quarter',angle=60,screen=screen) for screen in [50,100]]
    if os.environ.get('PG_HAIR_CLOSE_COMPARE') == '1':
        phases += [dict(stage=7,view='face',angle=angle,screen=100,hair_variant=variant)
                   for angle in [0,60] for variant in (['original','softened','fill'] if os.environ.get('PG_HAIR_FILL_COMPARE')=='1' else ['original','softened'])]
        if os.environ.get('PG_HAIR_CLOSE_ONLY') == '1':
            phases=[p for p in phases if 'hair_variant' in p]
if IMPROVED:
    phases += [dict(stage=7,view=view,angle=60,screen=screen,overlap=True)
               for view in ['face','quarter'] for screen in [50,67,100]]
if CONTROLS:
    assert IMPROVED
    phases=[dict(stage=7,view=view,angle=60,screen=100,control=control)
            for view,control in [('face','hair_off'),('face','hair_on'),('face','world_shadow_on'),('face','world_shadow_off'),('overview','far_quality')]]
report=dict(status='RUNNING',native=NATIVE,shots=[])
started=time.monotonic()
prepared=None
index=0
stopped=None
world=None
busy=False
overlap=None
hair_restore=[]
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert unreal.PGEditorProbeTools.begin_play_window(1280,720)


def finish(error=None):
    global stopped
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    (OUT/'render.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    level.editor_request_end_play()
    performance.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    stopped=time.monotonic()


def tick(dt):
    global prepared,index,world,busy,overlap
    if busy:return
    busy=True
    try:
        now=time.monotonic()
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if stopped is not None:
            if not world and now-stopped>3:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            return
        if now-started>360:raise RuntimeError('Temporal comparison timed out')
        if not world or now-started<20:return
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if not isinstance(pawn,unreal.PGShadingComparisonPawn):return
        if index==len(phases):finish();return
        phase=phases[index]
        if prepared is None:
            for mi,values in hair_restore:
                for key,value in values.items():mi.set_scalar_parameter_value(key,value)
            hair_restore.clear()
            if IMPROVED:
                source=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGToonPreviewActor) if a.actor_has_tag('PGShadingStage7'))
                if overlap is None:
                    overlap=unreal.PGToonPreviewActor.spawn_preview_actor(world,source.get_actor_transform())
                    overlap.set_actor_location(source.get_actor_location()+unreal.Vector(35,35,0),False,True)
                    mesh=overlap.skeletal_mesh_component
                    mesh.set_skeletal_mesh_asset(source.skeletal_mesh_component.get_skeletal_mesh_asset())
                    mesh.set_leader_pose_component(source.skeletal_mesh_component)
                    for i in range(source.skeletal_mesh_component.get_num_materials()):mesh.set_material(i,source.skeletal_mesh_component.get_material(i))
                    mesh.set_render_custom_depth(True);mesh.set_custom_depth_stencil_value(73)
                    for name in ['head_bone','head_forward_axis','head_right_axis','key_light']:
                        overlap.toon_presentation.set_editor_property(name,source.toon_presentation.get_editor_property(name))
                    overlap.toon_presentation.initialize(mesh)
                overlap.set_actor_hidden_in_game(not phase.get('overlap',False))
                proxy=source.toon_presentation.get_hair_shadow_proxy()
                assert proxy and proxy.static_mesh, 'Missing runtime static hair proxy'
            pawn.focus_stage(phase['stage'])
            if phase['view']=='overview':pawn.show_overview()
            elif phase['view']=='face':pawn.show_face()
            else:pawn.show_quarter()
            pawn.set_light_angles(phase['angle'],35)
            if SOFT_HAIR:
                pawn.get_component_by_class(unreal.CameraComponent).set_field_of_view(24 if 'hair_variant' in phase else 45)
            if 'hair_variant' in phase:
                source=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGToonPreviewActor)
                            if a.actor_has_tag('PGShadingStage7'))
                baseline=json.loads((ROOT/'Saved/HairSoftness/instances/hair.json').read_text())
                baseline={r['path']:dict(r['before'],HairNormalBlend=0.,WorldLightingInfluence=.65) for r in baseline}
                for i in range(source.skeletal_mesh_component.get_num_materials()):
                    mi=source.skeletal_mesh_component.get_material(i)
                    parent=mi
                    while isinstance(parent,unreal.MaterialInstanceDynamic):parent=parent.parent
                    path=parent.get_path_name().split('.')[0]
                    if path not in baseline:continue
                    assert mi.get_scalar_parameter_value('HairHeadFrameValid') == 1, path
                    assert mi.get_scalar_parameter_value('HairNormalBlend') > 0, path
                    if phase['hair_variant']=='original':
                        hair_restore.append((mi,{key:mi.get_scalar_parameter_value(key) for key in baseline[path]}))
                        for key,value in baseline[path].items():mi.set_scalar_parameter_value(key,value)
                    elif phase['hair_variant']=='fill':
                        hair_restore.append((mi,{'WorldLightingInfluence':mi.get_scalar_parameter_value('WorldLightingInfluence')}))
                        mi.set_scalar_parameter_value('WorldLightingInfluence',.25)
            if CONTROLS:
                if phase['control'].startswith('hair_'):pawn.toggle_hair_shadow()
                if phase['control'].startswith('world_shadow_'):pawn.toggle_shadow_caster()
            if HAIR:
                model=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGToonPreviewActor) if a.actor_has_tag('PGShadingStage7'))
                toon=model.toon_presentation
                toon.hair_shadow_mesh=unreal.load_asset('/Game/Art/ToonTest/Improvement/Hair/SM_PGBokusei_HairShadow') if phase['proxy']=='coarse' else None
                toon.initialize(model.skeletal_mesh_component)
                model.hair_shadow_proxy.set_cast_shadow(phase['proxy']=='cards')
                toon.key_light.light_component.set_editor_property('light_source_angle',phase['source'])
            for command in ['DisableAllScreenMessages','t.MaxFPS 60','r.AntiAliasingMethod 4',
                            'r.ScreenPercentage '+str(phase['screen']),'r.Streaming.FullyLoadUsedTextures 1']:
                unreal.SystemLibrary.execute_console_command(world,command)
            prepared=now
            return
        if now-prepared<2.5:return
        if phase.get('hair_variant')=='softened' and not report.get('hair_head_translation_pass'):
            # Wait for the existing distance-quality tick after moving the camera
            # close; a distant head is intentionally updated at only 15 Hz.
            source=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGToonPreviewActor)
                        if a.actor_has_tag('PGShadingStage7'))
            mi=next(source.skeletal_mesh_component.get_material(i) for i in range(source.skeletal_mesh_component.get_num_materials())
                    if source.skeletal_mesh_component.get_material(i).get_scalar_parameter_value('HairNormalBlend')>0)
            location=source.get_actor_location()
            try:
                source.set_actor_location(location+unreal.Vector(17,0,0),False,True)
                source.toon_presentation.refresh_presentation()
                center=mi.get_vector_parameter_value('HairHeadCenterWS')
                head=source.skeletal_mesh_component.get_socket_location(source.toon_presentation.head_bone)
                assert (unreal.Vector(center.r,center.g,center.b)-head).length()<.1, ('Hair center did not follow translation',center,head)
                report['hair_head_translation_pass']=True
            finally:
                source.set_actor_location(location,False,True)
                source.toon_presentation.refresh_presentation()
            prepared=now
            return
        name=f"Stage{phase['stage']+1}_{phase['view']}_L{phase['angle']}_SP{phase['screen']}"
        if HAIR:name+=f"_{phase['proxy']}_Source{phase['source']}"
        if phase.get('overlap'):name+='_Overlap'
        if 'hair_variant' in phase:name+='_'+phase['hair_variant']
        if CONTROLS:
            name+='_'+phase['control']
            source=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGToonPreviewActor) if a.actor_has_tag('PGShadingStage7'))
            cast=source.toon_presentation.get_hair_shadow_proxy().get_editor_property('cast_shadow')
            if phase['control']=='hair_off':assert not cast,'Quality update re-enabled disabled hair shadow'
            if phase['control']=='hair_on':assert cast,'Hair toggle did not restore shadow'
            if phase['control']=='far_quality':assert not cast,'Far view retained costly hair shadow'
        path=OUT/(name+'.png')
        assert unreal.PGEditorProbeTools.capture_game_viewport(world,str(path)),name
        report['shots'].append(dict(phase,path=str(path),bytes=path.stat().st_size))
        index+=1;prepared=None
    except BaseException:finish(traceback.format_exc())
    finally:busy=False


handle=unreal.register_slate_post_tick_callback(tick)

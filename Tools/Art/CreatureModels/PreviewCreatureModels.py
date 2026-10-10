"""Save a reusable gallery and render each imported material variant and action pose."""
import json,time,traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/CreatureModels'
CFG=json.loads((OUT/'configure.json').read_text())
if CFG['status']!='PASS':
    unreal.SystemLibrary.quit_editor()
    raise RuntimeError('ConfigureCreatureModels must pass before preview')
DEST='/Game/Art/CreatureModels';MAP=DEST+'/Maps/L_PG_CreatureModels'
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVEL=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
REPORT=dict(status='RUNNING',map=MAP,images=[])
def finish(error=None):
    REPORT['status']='FAIL' if error else 'PASS'
    if error:REPORT['error']=error
    (OUT/'preview.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    if 'handle' in globals():unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()
def build():
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    mode=unreal.load_asset('/Game/Art/ToonTest/Advanced/BP_PGToonLabGameMode')
    if mode:world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
    pp=ACTORS.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());pp.set_editor_property('unbound',True)
    settings=unreal.PostProcessSettings()
    for k,v in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,override_auto_exposure_bias=True,auto_exposure_bias=0.,override_bloom_intensity=True,bloom_intensity=.15,override_motion_blur_amount=True,motion_blur_amount=0.).items():settings.set_editor_property(k,v)
    pp.set_editor_property('settings',settings)
    floor=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-12))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    floor.set_actor_scale3d(unreal.Vector(70,55,.2))
    floor.static_mesh_component.set_material(0,unreal.load_asset('/Game/Art/ToonTest/Advanced/Materials/M_PGToonLabFloor'))
    for pitch,yaw,intensity in [(-45,-70,4.),(-30,110,1.5)]:
        light=ACTORS.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1000),unreal.Rotator(pitch=pitch,yaw=yaw))
        light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);light.light_component.set_intensity(intensity)
        if intensity<2:light.light_component.set_cast_shadows(False)
    bases=[];shots=[]
    for i,row in enumerate(CFG['models']):
        for j,path in enumerate(row['blueprints']):
            bp=unreal.load_asset(path)
            a=ACTORS.spawn_actor_from_class(bp.generated_class(),unreal.Vector((j-1)*1000,(i-1.5)*1000,0))
            a.set_actor_label(bp.get_name());comp=a.skeletal_mesh_component
            idle=unreal.load_asset(row['clips']['Idle_Battle' if row['name']=='MainPlant' else 'Idle'])
            comp.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT);comp.override_animation_data(idle,True,True,0.,1.);comp.set_update_animation_in_editor(True)
            bases.append(a);shots.append((bp.get_name(),a,idle,.3))
            if j==0:
                action=unreal.load_asset(row['clips']['Attack_1' if 'Attack_1' in row['clips'] else 'Attack'])
                shots.append((row['name']+'_Attack',a,action,.45))
                if row['name']=='Griffin':shots.append(('Griffin_Flight',a,unreal.load_asset(row['clips']['Fly_Zero']),.5))
    cam=ACTORS.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(2400,3500,2600))
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(cam.get_actor_location(),unreal.Vector(0,0,200)),False)
    cam.camera_component.set_editor_property('field_of_view',55.)
    cam.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
    for cmd in ('DisableAllScreenMessages','r.ScreenPercentage 100','r.Streaming.FullyLoadUsedTextures 1','viewmode lit'):unreal.SystemLibrary.execute_console_command(world,cmd)
    LEVEL.editor_set_game_view(True)
    return world,bases,shots,cam
def tick(dt):
    global index,prepared,pending,last
    try:
        now=time.monotonic()
        if now-started>600:return finish('Render timeout')
        if now-started<35 or now-last<2:return
        if pending:
            if not pending.exists() or pending.stat().st_size<10000:return
            REPORT['images'].append(str(pending));index+=1;prepared=False;pending=None;last=now
            if index==len(shots):return finish()
            return
        label,a,clip,fraction=shots[index]
        comp=a.skeletal_mesh_component
        if prepared:
            REPORT.setdefault('poses',{})[label]=[str(comp.get_socket_transform(comp.get_bone_name(i),unreal.RelativeTransformSpace.RTS_COMPONENT)) for i in range(comp.get_num_bones())]
            pending=OUT/(label+'.png')
            if pending.exists():pending.unlink()
            unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+pending.as_posix()+'"');last=now;return
        for other in bases:other.set_is_temporarily_hidden_in_editor(other!=a)
        comp=a.skeletal_mesh_component;comp.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT);comp.override_animation_data(clip,False,False,clip.get_play_length()*fraction,1.);comp.set_update_animation_in_editor(True)
        origin,extent=a.get_actor_bounds(False)
        radius=max(extent.x,extent.y,extent.z,50.)
        target=origin;loc=target+unreal.Vector(radius*2.3,radius*3.5,radius*1.6)
        cam.set_actor_location(loc,False,False);cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(loc,target),False)
        LEVEL.pilot_level_actor(cam,'None');prepared=True;last=now
    except Exception:finish(traceback.format_exc())
try:
    world,bases,shots,cam=build();index=0;prepared=False;pending=None
    started=last=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    handle=unreal.register_slate_post_tick_callback(tick)
except Exception:finish(traceback.format_exc())

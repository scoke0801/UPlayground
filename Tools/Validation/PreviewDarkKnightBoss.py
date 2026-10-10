"""Deterministic full-body pose review of the actual spawned Dark Knight."""
import json,os,time,traceback
from pathlib import Path
import unreal

OUT=Path(os.environ['PG_DARK_KNIGHT_RUN'])
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
assert unreal.PGEditorProbeTools.begin_play_window()
started=time.monotonic();stopped=None;phase='setup';at=0.;index=0;boss=player=camera=None
frames=[(key,f) for key in ('Sweep','Uppercut','Double','Fury') for f in (.1,.2,.25,.3,.35,.4,.45,.5,.55,.6,.7,.8)]
frames += [('Idle',.25),('Walk',.25),('Run',.25),('Death',.85)]
report=dict(status='RUNNING',frames=[])

def finish(error=None):
    global stopped
    if stopped is not None:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    else:unreal.log('PGDarkKnight preview PASS')
    (OUT/'motion-review.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global boss,player,camera,phase,at,index
    now=time.monotonic();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and now-stopped>4:
            boss=player=camera=None
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>240:finish('Timeout '+phase);return
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or now-started<5:return
            unreal.SystemLibrary.execute_console_command(world,'PGBossEncounter 15601');phase='find';return
        if phase=='find':
            found=[e for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if e.get_editor_property('character_tid')==15601]
            if not found:return
            boss=found[0];boss.get_controller().set_combat_thinking_enabled(False)
            boss.character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            boss.mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if w.is_in_viewport():w.remove_from_parent()
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            p=player.get_actor_location()
            p.z=p.z-player.capsule_component.get_scaled_capsule_half_height()+boss.capsule_component.get_scaled_capsule_half_height()
            boss.set_actor_location_and_rotation(p,unreal.Rotator(),False,True)
            player.set_actor_hidden_in_game(True)
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(),unreal.Rotator());assert camera
            pos=p+unreal.Vector(470,-540,260)
            camera.set_actor_location_and_rotation(pos,unreal.MathLibrary.find_look_at_rotation(pos,p),False,True)
            report['boss_location']=str(p);report['camera_location']=str(pos)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            phase='pose';at=t+3;return
        if t<at:return
        if phase=='pose':
            key,fraction=frames[index];clip=unreal.load_asset('/Game/DataCenter/DarkKnightBoss/AS_'+key)
            boss.mesh.play_animation(clip,False);boss.mesh.set_position(clip.get_play_length()*fraction,False);boss.mesh.set_play_rate(0.)
            at=t+.15;phase='capture';return
        if phase=='capture':
            key,fraction=frames[index];name=key+'_'+str(round(fraction*1000))+'.png'
            assert unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/name).as_posix()),name
            report['frames'].append(dict(key=key,fraction=fraction,path=name));index+=1
            if index==len(frames):finish()
            else:phase='pose';at=t+.05
    except Exception:finish(traceback.format_exc())

in_tick=False
def guarded_tick(dt):
    global in_tick
    if in_tick:return
    in_tick=True
    try:tick(dt)
    finally:in_tick=False
handle=unreal.register_slate_post_tick_callback(guarded_tick)

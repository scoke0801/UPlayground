"""Close P09 pose frames for authored-contact review; never saves assets/maps."""
import json
from pathlib import Path
import re
import time
import traceback
import unreal

command=unreal.SystemLibrary.get_command_line();assert '-PGTestProfile=' in command
OUT=Path(re.search(r'-PGHumanoidEvidence="?([^"\s]+)',command)[1])
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert level.load_level('/Game/Maps/RogueArena')
assert unreal.PGEditorProbeTools.begin_play_window()
started=time.monotonic();stopped=None;phase='setup';at=0.;index=0;boss=player=camera=None
spec=json.loads((Path(unreal.Paths.project_dir())/'Tools/Validation/Data/HumanoidBoss.json').read_text(encoding='utf-8'))
frames=[]
for key in ('Combo1','Combo2','Combo3','Thrust','Charge','Ring','Counter'):
    contact=spec['contact_fractions'][key]
    frames.extend((key,fraction) for fraction in (.15,contact-.04,contact,contact+.04,spec['end_fractions'].get(key,1.)))
report=dict(status='RUNNING',frames=[])

def finish(error=None):
    global stopped
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error('PGHumanoidMotion FAIL '+error)
    else:unreal.log('PGHumanoidMotion PASS')
    (OUT/'motion-review.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global boss,player,camera,phase,at,index
    now=time.monotonic();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped:
        if not world and now-stopped>4:
            boss=player=camera=None
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>180:finish('timeout '+phase);return
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or now-started<5:return
            unreal.SystemLibrary.execute_console_command(world,'PGBossEncounter 15401');phase='find';return
        if phase=='find':
            found=[e for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if e.get_editor_property('character_tid')==15401]
            if not found:return
            boss=found[0];boss.get_controller().set_combat_thinking_enabled(False)
            boss.character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            boss.mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if w.is_in_viewport():w.remove_from_parent()
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            p=boss.get_actor_location();player.set_actor_hidden_in_game(True)
            # Keep the review camera in PIE, so ending play owns all camera cleanup.
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(),unreal.Rotator())
            assert camera,'Could not spawn the PIE review camera'
            pos=p+unreal.Vector(280,-340,210)
            camera.set_actor_location_and_rotation(pos,unreal.MathLibrary.find_look_at_rotation(pos,p),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            phase='pose';at=t+2.;return
        if t<at:return
        if phase=='pose':
            key,fraction=frames[index];clip=unreal.load_asset('/Game/DataCenter/HumanoidBoss/AS_'+key)
            boss.mesh.play_animation(clip,False);boss.mesh.set_position(clip.get_play_length()*fraction,False);boss.mesh.set_play_rate(0.)
            at=t+.2;phase='capture';return
        if phase=='capture':
            key,fraction=frames[index];name=key+'_'+str(round(fraction*1000))+'.png'
            assert unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/name).as_posix()),'Empty motion capture: '+name
            report['frames'].append(dict(key=key,fraction=fraction,contact=abs(fraction-spec['contact_fractions'][key])<1.e-6,
                path=name,position=str(boss.get_actor_location()),camera=str(camera.get_actor_location())));index+=1
            if index==len(frames):phase='done';at=t+.3
            else:phase='pose';at=t+.1
        elif phase=='done':finish()
    except Exception:finish(traceback.format_exc())

in_tick=False
def guarded_tick(dt):
    global in_tick
    if in_tick:return
    in_tick=True
    try:tick(dt)
    finally:in_tick=False

handle=unreal.register_slate_post_tick_callback(guarded_tick)

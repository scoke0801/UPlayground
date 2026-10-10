"""Rendered continuous movement on actual player/P09 animation graphs in isolated PIE."""
import json
import math
import os
from pathlib import Path
import sys
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterCatalog import PLAYER_IDS
OUT=Path(os.environ['PG_LOCOMOTION_RUN'])
assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
placed=[]
for i in range(15201,15207):
    actor=editor.spawn_actor_from_class(unreal.load_asset('/Game/DataCenter/MonsterVariations/BP_'+str(i)).generated_class(),unreal.Vector(500,500,200))
    actor.tags=['PGLocomotionProbe'];placed.append(actor)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert unreal.PGEditorProbeTools.begin_play_window()
started=time.monotonic();stopped=None;phase='setup';index=0;at=0.;player=None;camera=None;active=None;actors={};samples=[];captures=0
cases=[(identity,angle,600) for identity in PLAYER_IDS for angle in ([0,90,180,-90] if identity=='Bokusei' else [0])]
cases += [(i,angle,300 if i>=15205 else 380) for i in range(15201,15207) for angle in ([0,90,180] if i in (15201,15205) else [0])]
report={'status':'RUNNING','cases':[],'direct_keyboard_input':False,'bounded_preview_movement':True}

def finish(error=None):
    global stopped,player,camera,active,actors
    if stopped:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error('PGHumanoidLocomotion PREVIEW FAIL '+error)
    else:unreal.log('PGHumanoidLocomotion PREVIEW PASS')
    (OUT/'preview.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    player=camera=active=None;actors={}
    level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global phase,index,at,player,camera,active,actors,samples,captures
    now=time.monotonic();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped:
        if not world and now-stopped>3:
            for actor in placed:editor.destroy_actor(actor)
            placed.clear();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>220:finish('Timeout '+phase);return
    if not world:return
    try:
        if phase=='setup':
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or now-started<7:return
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world,'PGStress 0 0')
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            unreal.SystemLibrary.execute_console_command(world,'r.MotionBlurQuality 0')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if widget.is_in_viewport():widget.remove_from_parent()
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                if actor.get_controller():actor.get_controller().set_combat_thinking_enabled(False);actor.get_controller().stop_movement()
                if actor.actor_has_tag('PGLocomotionProbe'):actors[actor.get_editor_property('character_tid')]=actor
            assert len(actors)==6
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(),unreal.Rotator())
            camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(40.)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            # Pause character aim updates, leaving movement and animation components running.
            for actor in [player]+list(actors.values()):
                actor.set_actor_tick_enabled(False)
                if actor.get_controller(): actor.get_controller().set_actor_tick_enabled(False)
                actor.set_editor_property('use_controller_rotation_yaw',False)
                actor.set_actor_enable_collision(False)
                actor.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
                actor.character_movement.set_editor_property('orient_rotation_to_movement',False)
                actor.character_movement.set_editor_property('use_controller_desired_rotation',False)
                actor.character_movement.set_editor_property('max_acceleration',6000.)
                actor.set_actor_hidden_in_game(True)
            phase='begin'
        if phase=='begin':
            identity,angle,speed=cases[index]
            active=actors[identity] if isinstance(identity,int) else player
            for actor in [player]+list(actors.values()):
                actor.character_movement.stop_movement_immediately();actor.set_actor_hidden_in_game(actor!=active)
            if active==player:
                assert player.appearance_component.apply_appearance(unreal.load_asset('/Game/DataCenter/Characters/DA_'+identity))
            active.set_actor_location_and_rotation(unreal.Vector(800,800,active.capsule_component.get_scaled_capsule_half_height()),unreal.Rotator(),False,True)
            active.character_movement.set_editor_property('max_fly_speed',float(speed))
            samples=[];captures=0;at=now;phase='move'
        if phase=='move':
            identity,angle,speed=cases[index]
            direction=unreal.MathLibrary.transform_direction(active.get_actor_transform(),unreal.Vector(math.cos(math.radians(angle)),math.sin(math.radians(angle)),0))
            active.add_movement_input(direction,1.,True)
            position=active.get_actor_location()
            # Keep the close review camera away from arena perimeter walls. Movement
            # and the live graph continue; only this disposable fixture recenters.
            if math.hypot(position.x-800,position.y-800)>200:
                active.set_actor_location(unreal.Vector(800,800,position.z),False,True)
                position=active.get_actor_location()
            focus=position+unreal.Vector(0,0,0)
            camera_pos=focus+unreal.Vector(380,-520,220)
            camera.set_actor_location_and_rotation(camera_pos,unreal.MathLibrary.find_look_at_rotation(camera_pos,focus),False,True)
            visual=active.appearance_component.get_presentation_mesh()
            # Combat body has a stable bone vocabulary across all displayed rigs.
            point=active.mesh.get_socket_location('foot_l')-position
            samples.append([point.x,point.y,point.z])
            elapsed=now-at
            if captures<4 and elapsed>=.8+captures*.18:
                name=f'{identity}_{angle}_{captures}.png'
                if unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/name).as_posix()): captures+=1
                else: assert elapsed<4.,(identity,angle,'Viewport remained empty')
            if elapsed>1.6 and captures==4:
                travel=max(math.dist(p,samples[len(samples)//2]) for p in samples[len(samples)//2:])
                assert travel>4.,(identity,angle,'Frozen feet',travel)
                actual=active.get_velocity().length()
                assert actual>speed*.85,(identity,'No movement',actual)
                assert visual and visual.get_anim_instance()
                appearance=active.appearance_component.get_appearance()
                head=visual.get_socket_transform(appearance.get_editor_property('head_bone'))
                forward=unreal.MathLibrary.transform_direction(head,appearance.get_editor_property('head_forward_axis'))
                actor_forward=active.get_actor_forward_vector()
                facing_dot=forward.x*actor_forward.x+forward.y*actor_forward.y
                assert facing_dot>.2,(identity,'Reversed visible facing',facing_dot)
                report['cases'].append(dict(identity=identity,angle=angle,speed=actual,foot_travel_cm=travel,captures=captures,
                                           facing_dot=facing_dot,actor_rotation=str(active.get_actor_rotation()),
                                           anim=active.mesh.get_anim_instance().get_class().get_path_name()))
                index+=1
                if index==len(cases):finish()
                else:phase='begin'
    except Exception:finish(traceback.format_exc())

busy=False
def guarded_tick(dt):
    global busy
    if busy:return
    busy=True
    try:tick(dt)
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(guarded_tick)

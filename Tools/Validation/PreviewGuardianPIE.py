"""Rendered guardian states, real hit routes and authored stage-3/wave-3 composition.
Uses an isolated test profile, no map/blueprint saves and no human input claim.
"""
import json
import re
import time
import traceback
from pathlib import Path
import unreal

command=unreal.SystemLibrary.get_command_line()
assert '-PGTestProfile=' in command
match=re.search(r'-PGGuardianOut="?([^"\s]+)',command)
OUT=Path(match[1]) if match else Path(unreal.Paths.project_saved_dir())/'Guardian/Preview'
OUT.mkdir(parents=True,exist_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
def rows(path): return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))
enemies={r['EnemyID']:r for r in rows('/Game/DataCenter/DataTables/Actor/DT_Enemy')}
stage=next(r for r in rows('/Game/DataCenter/DataTables/Stage/DT_StageData') if r['Id']==3)
pack=stage['Waves'][2]['MonsterSpawnInfos']
assert {(r['MonsterId'],r['SpawnCount']) for r in pack} == {(15103,2),(15102,3)}
placed=[]
for row in pack:
    for index in range(row['SpawnCount']):
        actor=editor.spawn_actor_from_class(unreal.load_class(None,enemies[row['MonsterId']]['ActorClass']),unreal.Vector(1400,1400,100))
        actor.set_editor_property('tags',['PGGuardianPack',f"PGGuardianRole_{row['MonsterId']}"]+(['PGGuardianPrimary'] if row['MonsterId']==15103 and index==0 else []))
        placed.append(actor)
camera_template=editor.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(1400,1400,600))
camera_template.set_editor_property('tags',['PGGuardianCamera'])
placed.append(camera_template)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level.editor_request_begin_play()
started=time.monotonic(); at=0.; phase=0; stopped=None
player=guardian=presentation=camera=None
pack_actors=[]; samples=[]; report=dict(status='RUNNING',assisted=True,direct_input=False,scripted_movement=True,stage=3,wave=3,composition=pack)

def console(world,text): unreal.SystemLibrary.execute_console_command(world,text)
def hide(actor,value):
    actor.set_actor_hidden_in_game(value); actor.set_actor_enable_collision(not value)
    for child in actor.get_attached_actors(): child.set_actor_hidden_in_game(value)
def sample(state):
    row=dict(state=state,guard=guardian.get_editor_property('guarding'),active=guardian.get_editor_property('pattern_active'),
             recovery=guardian.get_editor_property('pattern_recovering'),locked=presentation.is_aim_locked(),
             exposed=round(presentation.get_exposure_alpha(),3),armor=presentation.get_armor_count())
    samples.append(row); unreal.log('PGGuardianPreview sample='+json.dumps(row))
    mesh=guardian.get_editor_property('mesh')
    row['hand_pose']=str(mesh.get_socket_transform('hand_l',unreal.RelativeTransformSpace.RTS_COMPONENT))
    row['right_hand_pose']=str(mesh.get_socket_transform('hand_r',unreal.RelativeTransformSpace.RTS_COMPONENT))
    row['speed']=round(guardian.get_velocity().length(),3)
    row['anim_class']=mesh.get_anim_instance().get_class().get_path_name()
    console(guardian,f'HighResShot 1280x720 filename="{(OUT/(state+".png")).as_posix()}"')
def finish(world,error=None):
    global stopped,player,guardian,presentation,camera
    if stopped is not None:return
    report.update(status='FAIL' if error else 'PASS',samples=samples)
    if error: report['error']=error; unreal.log_error('PGGuardianPreview FAIL '+error)
    else: unreal.log('PGGuardianPreview PASS '+json.dumps(report))
    (OUT/'observations.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if world: unreal.GameplayStatics.set_global_time_dilation(world,1.)
    player=guardian=presentation=camera=None; pack_actors.clear()
    level.editor_request_end_play(); stopped=time.monotonic()

def tick(dt):
    global phase,at,player,guardian,presentation,camera,origin,motion_deadline
    now=time.monotonic()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and placed:
            for actor in placed: editor.destroy_actor(actor)
            placed.clear(); unreal.SystemLibrary.collect_garbage()
        if not world and now-stopped>3:
            unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor()
        return
    if now-started>110: finish(world,'Timed out'); return
    if not world or now<at:return
    try:
        if phase==0:
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or now-started<6:return
            console(world,'t.MaxFPS 60'); console(world,'PGStress 0 0'); console(world,'DisableAllScreenMessages')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if widget.is_in_viewport() and not isinstance(widget,unreal.PGUIMainHUD):widget.remove_from_parent()
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                if actor.actor_has_tag('PGGuardianPack'):
                    pack_actors.append(actor); actor.get_controller().set_combat_thinking_enabled(False); hide(actor,True)
                    if actor.actor_has_tag('PGGuardianPrimary'):guardian=actor
            assert len(pack_actors)==5 and guardian
            presentation=guardian.get_component_by_class(unreal.PGEnemyPresentationComponent)
            assert presentation.get_armor_count()==3
            armor=[c for c in guardian.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('PG_Guardian')]
            assert len(armor)==3 and all(c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for c in armor)
            origin=player.get_actor_location()
            guardian.set_actor_location_and_rotation(unreal.Vector(origin.x-700,origin.y,origin.z),unreal.Rotator(),False,True)
            hide(guardian,False); guardian.get_controller().set_combat_thinking_enabled(True)
            motion_deadline=now+10.
            phase=.5; at=now+.5
        elif phase==.5:
            guardian.get_controller().set_combat_thinking_enabled(False)
            phase=1
        elif phase==1:
            # Feed CharacterMovement for an isolated animation sample. Free AI is
            # exercised separately by the mixed pack; this is not player input QA.
            guardian.add_movement_input(unreal.Vector(1,0,0),1.,True)
            if guardian.get_velocity().length()<50:
                movement=guardian.get_editor_property('character_movement')
                assert now<motion_deadline,('Guardian never entered locomotion',str(guardian.get_actor_location()),
                    str(movement.get_editor_property('movement_mode')),movement.get_editor_property('max_walk_speed'))
                return
            assert guardian.get_editor_property('mesh').get_anim_instance().get_class().get_path_name()=='/Game/DataCenter/Guardian/ABP_PGGuardian.ABP_PGGuardian_C'
            sample('walking')
            phase=1.5; at=now+.15
        elif phase==1.5:
            guardian.get_controller().set_combat_thinking_enabled(False)
            guardian.set_actor_location_and_rotation(unreal.Vector(origin.x-360,origin.y,origin.z),unreal.Rotator(),False,True)
            # Close inspection camera, then return to the real gameplay camera for the pack.
            location=guardian.get_actor_location()+unreal.Vector(220,-300,240)
            target=guardian.get_actor_location()+unreal.Vector(0,0,25)
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if a.actor_has_tag('PGGuardianCamera'))
            camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,target),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0)
            phase=2; at=now+4.5
        elif phase==2:
            assert guardian.get_editor_property('guarding') and presentation.get_exposure_alpha()<.01
            hand=guardian.get_editor_property('mesh').get_socket_transform('hand_l',unreal.RelativeTransformSpace.RTS_COMPONENT).translation
            assert hand.y>20 and hand.z>100,'Guard idle did not select the raised shield arm'
            sample('guard')
            phase=2.5; at=now+.35
        elif phase==2.5:
            player.set_actor_location(guardian.get_actor_location()+unreal.Vector(200,0,0),False,True)
            console(world,'PGGuardianProbe hit')
            phase=3; at=now+.5
        elif phase==3:
            player.set_actor_location(guardian.get_actor_location()+unreal.Vector(-200,0,0),False,True)
            console(world,'PGGuardianProbe hit')
            phase=4; at=now+.5
        elif phase==4:
            player.set_actor_location(guardian.get_actor_location()+unreal.Vector(240,0,0),False,True)
            assert guardian.get_controller().try_execute_skill(15103)
            phase=5; at=now+.23
        elif phase==5:
            assert guardian.get_editor_property('pattern_active') and not presentation.is_aim_locked()
            sample('windup'); phase=6; at=now+.35
        elif phase==6:
            assert presentation.is_aim_locked()
            sample('locked'); phase=7
        elif phase==7:
            if not guardian.get_editor_property('pattern_recovering'):return
            sample('impact')
            phase=8; at=now+.28
        elif phase==8:
            assert not guardian.get_editor_property('guarding') and presentation.get_exposure_alpha()>.99
            sample('recovery'); phase=8.5; at=now+.25
        elif phase==8.5:
            console(world,'PGGuardianProbe hit')
            phase=9; at=now+4.5
        elif phase==9:
            assert guardian.get_controller().try_execute_skill(15103)
            phase=10; at=now+.15
        elif phase==10:
            guardian.get_controller().set_combat_thinking_enabled(False)
            phase=11; at=now+.5
        elif phase==11:
            assert not guardian.get_editor_property('pattern_active') and not presentation.is_aim_locked()
            sample('cancelled')
            # Start all five from a readable formation using the existing wave's composition.
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(player,0)
            player.set_actor_location(origin,False,True)
            positions=[(-300,-150),(-350,180),(-650,-280),(-750,60),(-610,350)]
            ordered=sorted(pack_actors,key=lambda a:not a.actor_has_tag('PGGuardianRole_15103'))
            for actor,(x,y) in zip(ordered,positions):
                actor.set_actor_location_and_rotation(origin+unreal.Vector(x,y,0),unreal.Rotator(),False,True)
                hide(actor,False); actor.get_controller().set_combat_thinking_enabled(True)
            phase=12; at=now+2
        elif phase==12:
            sample('mixed_wave'); phase=13; at=now+5
        elif phase==13:
            sample('mixed_wave_late')
            phase=13.5; at=now+.3
        elif phase==13.5:
            for actor in pack_actors:actor.get_controller().set_combat_thinking_enabled(False)
            console(world,'PGGuardianProbe kill'); phase=14; at=now+.1
        elif phase==14:
            assert not guardian.get_editor_property('pattern_active') and not presentation.is_aim_locked()
            report['death_cleared_pattern']=True
            phase=15; at=now+5
        elif phase==15:
            remaining=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if a.actor_has_tag('PGGuardianPrimary')]
            assert not remaining,'Dead guardian and its attached armor did not clear'
            report['death_removed_actor_and_armor']=True
            finish(world)
    except Exception:
        finish(world,traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)

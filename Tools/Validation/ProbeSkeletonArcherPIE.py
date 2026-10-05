"""Observe both real GAS archer casts, montage timing, single arrow and cleanup."""
import json
from pathlib import Path
import re
import sys
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureSkeletonArcher import validate, ARROW, MONTAGE
validate()
command=unreal.SystemLibrary.get_command_line()
assert '-PGTestProfile=' in command
OUT=Path(re.search(r'-PGArcherEvidence="?([^"\s]+)',command)[1])
OUT.mkdir(parents=True,exist_ok=True)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
editor_actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
enemies=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')))
skills={r['SkillID']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')))}
row=next(r for r in enemies if r['EnemyID']==15102)
placed=editor_actors.spawn_actor_from_class(unreal.load_class(None,row['ActorClass']),unreal.Vector(1400,1400,100))
placed.set_editor_property('tags',['PGArcherProbe'])
placed_camera=editor_actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,0,500))
placed_camera.set_editor_property('tags',['PGArcherCamera'])
level.editor_request_begin_play()
started=time.monotonic()
stopped=None
enemy=player=camera=None
phase='setup'
at=cast_at=0.
freeze_at=0.
index=0
observed=set()
sample={}
report=dict(status='RUNNING',assisted=True,direct_input=False,casts=[])


def capture(world,name):
    unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+(OUT/(name+'.png')).as_posix()+'"')


def finish(error=None):
    global stopped,enemy,player,camera
    if stopped is not None: return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world: unreal.GameplayStatics.set_global_time_dilation(world,1.)
    report['status']='FAIL' if error else 'PASS'
    if error: report['error']=error; unreal.log_error('PGArcherProbe FAIL '+error)
    else: unreal.log('PGArcherProbe PASS '+json.dumps(report))
    (OUT/'presentation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    enemy=player=camera=None
    level.editor_request_end_play()
    stopped=time.monotonic()


def tick(dt):
    global enemy,player,camera,placed,placed_camera,phase,at,cast_at,index,observed,sample,freeze_at
    now=time.monotonic()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and placed:
            editor_actors.destroy_actor(placed); placed=None
            editor_actors.destroy_actor(placed_camera); placed_camera=None
            unreal.SystemLibrary.collect_garbage()
        if not world and now-stopped>3:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now-started>150: finish('Timeout phase='+phase); return
    if not world: return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or now-started<6: return
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world,'PGStress 0 0')
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            unreal.SystemLibrary.execute_console_command(world,'r.MotionBlurQuality 0')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if widget.is_in_viewport(): widget.remove_from_parent()
            enemy=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if a.actor_has_tag('PGArcherProbe'))
            enemy.get_controller().set_combat_thinking_enabled(False)
            origin=player.get_actor_location()
            pos=unreal.Vector(origin.x-650,origin.y,origin.z)
            enemy.set_actor_location_and_rotation(pos,unreal.Rotator(),False,True)
            location=pos+unreal.Vector(220,-650,300)
            target=pos+unreal.Vector(180,0,30)
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if a.actor_has_tag('PGArcherCamera'))
            camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,target),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            phase='start'; at=t+7.
            return
        sid=(15102,15112)[index]
        row=skills[sid]
        if phase=='start':
            if t<at: return
            pos=enemy.get_actor_location()
            location=pos+unreal.Vector(220,-650,300)
            camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,pos+unreal.Vector(180,0,30)),False,True)
            assert not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGPatternProjectile)
            assert enemy.get_controller().try_execute_skill(sid), 'Activation '+str(sid)
            cast_at=t; observed=set(); sample=dict(skill=sid,montage_samples=[])
            phase='windup'
            return
        projectiles=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGProjectileBase)
        observed.update(p.get_path_name() for p in projectiles)
        assert len(observed)<=1, 'Extra projectile/legacy notify '+str(observed)
        anim=enemy.get_editor_property('mesh').get_anim_instance()
        montage=anim.get_current_active_montage()
        age=t-cast_at
        if phase=='windup':
            assert montage and montage.get_path_name().split('.')[0]==MONTAGE, 'Wrong montage '+str(montage)
            position=anim.montage_get_position(montage)
            sample['montage_samples'].append([age,position])
            if age>=row['TelegraphDuration']*.6:
                assert not projectiles, 'Arrow released before aim windup'
                expected=montage.get_play_length()*row['ImpactMontageFraction']*age/row['TelegraphDuration']
                assert abs(position-expected)<.07, (position,expected)
                capture(world,str(sid)+'_aim')
                phase='release'
        elif phase=='release':
            if not projectiles: return
            assert len(projectiles)==1
            bolt=projectiles[0]
            assert isinstance(bolt,unreal.PGPatternProjectile)
            mesh=bolt.get_editor_property('mesh_component')
            assert mesh.get_editor_property('static_mesh').get_path_name().split('.')[0]==ARROW
            box=bolt.get_editor_property('projectile_collision_box').get_unscaled_box_extent()
            assert abs(box.y-row['LineHalfWidth'])<.001
            velocity=bolt.get_editor_property('movement_component').get_editor_property('velocity')
            assert abs(velocity.length()-row['TravelSpeed'])<1.
            assert velocity.x>0 and abs(velocity.y)<1.
            assert abs(age-row['TelegraphDuration'])<.12, (age,row['TelegraphDuration'])
            assert montage and montage.get_path_name().split('.')[0]==MONTAGE
            position=anim.montage_get_position(montage)
            release=montage.get_play_length()*row['ImpactMontageFraction']
            assert abs(position-release)<.12,(position,release)
            sample.update(release_seconds=age,release_pose_seconds=position,authored_release_seconds=release,
                          mesh=ARROW,speed=velocity.length(),collision_half_width=box.y)
            phase='flight'; at=t+.12
        elif phase=='flight':
            if t<at: return
            assert len(projectiles)==1, 'Arrow disappeared before flight sample'
            bolt=projectiles[0]
            mesh=bolt.get_editor_property('mesh_component')
            assert (mesh.get_world_location()-bolt.get_actor_location()).length()<.1
            assert mesh.get_right_vector().dot(bolt.get_actor_forward_vector())>.99, 'Arrow tip is not forward'
            sample['flight_location']=str(bolt.get_actor_location())
            unreal.GameplayStatics.set_global_time_dilation(world,.001)
            phase='freeze'; freeze_at=now+.25
        elif phase=='freeze':
            if now<freeze_at: return
            capture(world,str(sid)+'_flight')
            phase='close'; freeze_at=now+.4
        elif phase=='close':
            if now<freeze_at: return
            target=projectiles[0].get_actor_location()
            location=target+unreal.Vector(30,-190,65)
            camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,target),False,True)
            capture(world,str(sid)+'_arrow_detail')
            phase='unfreeze'; freeze_at=now+.5
        elif phase=='unfreeze':
            if now<freeze_at: return
            unreal.GameplayStatics.set_global_time_dilation(world,1.)
            phase='cleanup'; at=cast_at+row['TelegraphDuration']+row['RecoveryDuration']+1.
        elif phase=='cleanup':
            if t<at: return
            assert not projectiles and len(observed)==1
            assert not enemy.get_editor_property('pattern_active')
            sample.update(projectile_count=len(observed),cleared=True)
            report['casts'].append(sample)
            index+=1
            if index==2: finish(); return
            phase='start'; at=t+1.
    except Exception:
        finish(traceback.format_exc())


handle=unreal.register_slate_post_tick_callback(tick)

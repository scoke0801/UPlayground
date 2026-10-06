"""All authored monsters: live GAS stats, motion, loadouts, damage and recovery."""
import json
import os
from pathlib import Path
import re
import sys
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureMonsterVariations import validate,rows,TABLES
from MonsterVariationRoster import SPEC,IDS,P09_IDS
validate()
command=unreal.SystemLibrary.get_command_line()
assert '-PGTestProfile=' in command
OUT=Path(re.search(r'-PGMonsterEvidence="?([^"\s]+)',command)[1])
render='-nullrhi' not in command
enemies={r['EnemyID']:r for r in rows(TABLES['enemies'])}
skills={r['SkillID']:r for r in rows(TABLES['skills'])}
stats={r['CharacterID']:{k:(v['Stats'] if isinstance(v,dict) else v) for k,v in r['Stats'].items()} for r in rows(TABLES['stats'])}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
placed=[]
for index,eid in enumerate(sorted(IDS)):
    actor=editor.spawn_actor_from_class(unreal.load_class(None,enemies[eid]['ActorClass']),unreal.Vector(700+(index%4)*260,700+(index//4)*260,200))
    actor.set_editor_property('tags',['PGMonsterProbe'])
    actor.get_editor_property('character_movement').set_movement_mode(unreal.MovementMode.MOVE_NONE)
    placed.append(actor)
camera_placed=editor.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,0,800))
camera_placed.set_editor_property('tags',['PGMonsterCamera'])
placed.append(camera_placed)
level.editor_request_begin_play()
started=time.monotonic()
stopped=None
phase='setup'
index=0
at=0.
actors={}
player=camera=None
cases=[(eid,sid) for eid in sorted(IDS) for sid in enemies[eid]['SkillIdList']]
only=re.search(r'-PGMonsterOnly=(\d+)',command)
if only:cases=[case for case in cases if case[0]==int(only[1])]
report=dict(status='RUNNING',assisted=True,direct_input=False,loadouts=[],casts=[])

def attribute(actor,name):
    attr=unreal.GameplayAttribute()
    attr.import_text('(Attribute="/Script/PGAbilitySystem.PGAtrributeSet:'+name+'")')
    value,found=unreal.AbilitySystemLibrary.get_float_attribute(actor,attr)
    assert found,name
    return value

def finish(error=None):
    global stopped,actors,player,camera
    if stopped is not None:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error('PGMonsterProbe FAIL '+error)
    else:unreal.log('PGMonsterProbe PASS')
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    actors={};player=camera=None
    level.editor_request_end_play()
    stopped=time.monotonic()

def tick(dt):
    global phase,index,at,actors,player,camera,health_before,cast_at,pose_before
    now=time.monotonic()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if now-stopped>4 and placed:
            for actor in placed:editor.destroy_actor(actor)
            placed.clear()
        if now-stopped>5:
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>280:finish('Timeout '+phase);return
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            if now-started<7:return
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player:return
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world,'PGStress 0 0')
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if widget.is_in_viewport():widget.remove_from_parent()
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                actor.get_controller().set_combat_thinking_enabled(False)
                if actor.actor_has_tag('PGMonsterProbe'):
                    eid=actor.get_editor_property('character_tid');actors[eid]=actor
                    # NullRHI has no rendered frames; the pose-motion assertion still needs bones.
                    actor.mesh.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
                    assert abs(attribute(actor,'MaxHealth')-stats[eid]['Health'])<.1,eid
                    assert abs(attribute(actor,'AttackPower')-stats[eid]['Attack'])<.1,eid
                    if eid in P09_IDS:
                        visual=actor.appearance_component.get_presentation_mesh()
                        assert visual and not visual.get_editor_property('hidden_in_game')
                        gear=[c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==visual]
                        appearance=actor.appearance_component.get_appearance()
                        assert len(gear)==len(appearance.get_editor_property('attachments'))
                        for part in gear:
                            assert part.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
                            assert (part.get_world_location()-visual.get_socket_location(part.get_attach_socket_name())).length()<50
                        report['loadouts'].append(dict(enemy=eid,gear=[dict(mesh=c.static_mesh.get_path_name(),scale=str(c.get_world_scale()),bounds=str(c.get_local_bounds())) for c in gear]))
                        # Swapping the cosmetic identity must remove old gear, then restore once.
                        base=unreal.load_asset('/Game/DataCenter/Characters/DA_P09_'+('Female' if eid%2 else 'Male'))
                        assert actor.appearance_component.apply_appearance(base)
                        assert not [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==actor.appearance_component.get_presentation_mesh()]
                        assert actor.appearance_component.apply_appearance(appearance)
                        restored=[c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==actor.appearance_component.get_presentation_mesh()]
                        assert len(restored)==len(gear)
            assert set(actors)==IDS, sorted(actors)
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if a.actor_has_tag('PGMonsterCamera'))
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            phase='position';at=t+3;return
        eid,sid=cases[index]
        enemy=actors[eid];row=skills[sid]
        if phase=='position':
            if t<at:return
            # Isolate the current combatant; no invisible actors within hit queries.
            for otherid,actor in actors.items():
                if otherid!=eid:
                    actor.set_actor_hidden_in_game(True)
                    actor.get_editor_property('character_movement').set_movement_mode(unreal.MovementMode.MOVE_NONE)
                    actor.set_actor_location(unreal.Vector(700+(otherid%4)*260,1400,200),False,True)
            enemy.get_editor_property('character_movement').set_movement_mode(unreal.MovementMode.MOVE_WALKING)
            enemy.set_actor_hidden_in_game(False)
            origin=player.get_actor_location()
            distance=550 if row['Pattern'] in ('AimedProjectile','HazardSequence','ChargeSlam') else 130
            if row['Pattern']=='RingBurst':distance=(row['InnerSafeRadius']+row['TelegraphRadius'])*.5
            # Exercise the real AI gate at a legal range, including thrust's dead zone.
            maximum=row['SkillRange'] or 10000.
            if row['Pattern'] in ('Sweep','RingBurst'):maximum=min(maximum,row['TelegraphRadius'])
            if row['Pattern'] in ('AimedProjectile','Thrust'):maximum=min(maximum,row['TravelDistance'])
            minimum=row.get('MinimumActivationRange',0.)
            assert minimum<maximum,(eid,sid,'invalid activation interval')
            distance=max(minimum+min(10.,(maximum-minimum)*.25),min(distance,maximum-5.))
            half=enemy.capsule_component.get_scaled_capsule_half_height()
            ground=origin.z-player.capsule_component.get_scaled_capsule_half_height()
            pos=unreal.Vector(origin.x-distance,origin.y,ground+half)
            enemy.set_actor_location_and_rotation(pos,unreal.Rotator(),False,True)
            location=pos+unreal.Vector(100,-360 if eid in P09_IDS else -620,150 if eid in P09_IDS else 340)
            camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,pos+unreal.Vector(0,0,10)),False,True)
            phase='start';at=t+1.;return
        if phase=='start':
            if t<at:return
            health_before=attribute(player,'CurrentHealth')
            pose_before={bone:enemy.mesh.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT).translation for bone in ('hand_r','hand_l','head')} if eid==15305 else {}
            assert enemy.get_controller().try_execute_skill(sid),(eid,sid,'activation',row.get('MinimumActivationRange'),(enemy.get_actor_location()-player.get_actor_location()).length())
            cast_at=t;phase='windup';return
        if phase=='windup':
            if t-cast_at<row['TelegraphDuration']*.55:return
            anim=enemy.mesh.get_anim_instance()
            montage=anim.get_current_active_montage()
            assert montage and montage.get_path_name()==row['ElitePresentationMontage'],(eid,sid,'montage')
            assert anim.montage_get_position(montage)>0
            if eid==15305:
                pose_delta=max((enemy.mesh.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT).translation-before).length() for bone,before in pose_before.items())
                assert pose_delta>2.,('Golem montage clock advanced without a visible pose',pose_delta)
                report.setdefault('golem_pose_deltas',[]).append(pose_delta)
            if render:unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+(OUT/(str(eid)+'_'+str(sid)+'.png')).as_posix()+'"')
            phase='finish_cast';return
        if phase=='finish_cast':
            if enemy.get_editor_property('pattern_active'):return
            if row['Pattern']=='AimedProjectile' and t-cast_at<row['TelegraphDuration']+550/row['TravelSpeed']+.5:return
            damage=health_before-attribute(player,'CurrentHealth')
            assert damage>0,(eid,sid,'no damage')
            assert not enemy.get_editor_property('pattern_recovering')
            report['casts'].append(dict(enemy=eid,skill=sid,damage=damage,duration=t-cast_at))
            unreal.log('PGMonsterProbe CAST '+json.dumps(report['casts'][-1]))
            index+=1
            if index==len(cases):finish();return
            phase='position';at=t+max(1.,enemies[eid].get('MinimumCombatWait',0.))
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)

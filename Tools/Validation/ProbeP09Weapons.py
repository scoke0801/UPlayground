"""Rendered P09 loadouts, all GAS casts and autonomous use of all nine attacks."""
import json
import os
from pathlib import Path
import re
import sys
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureP09Weapons import validate, rows, TABLES, SPEC, IDS
validate()
OUT=Path(os.environ['PG_P09_WEAPON_RUN'])
command=unreal.SystemLibrary.get_command_line();assert '-PGTestProfile=' in command
render='-nullrhi' not in command
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
enemies={r['EnemyID']:r for r in rows(TABLES['enemies'])};skills={r['SkillID']:r for r in rows(TABLES['skills'])}
definitions={m['id']:m for m in SPEC['monsters']}
only=re.search(r'-PGP09WeaponEnemy=(\d+)',command)
ids={int(only[1])} if only else IDS
cases=[(eid,sid) for eid in sorted(ids) for sid in enemies[eid]['SkillIdList']]
auto_cases=[(next(m['id'] for m in SPEC['monsters'] if m['family']==s['family'] and m['grade']=='Elite' and m['sex']=='Female'),s['id']) for s in SPEC['skills']] if not only else []
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert level.load_level('/Game/Maps/RogueArena')
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
placed=[]
for i,eid in enumerate(sorted(ids)):
    a=editor.spawn_actor_from_class(unreal.load_class(None,enemies[eid]['ActorClass']),unreal.Vector(1300+(i%5)*180,1300+(i//5)*180,200))
    a.set_editor_property('tags',['PGP09Weapon']);placed.append(a)
del a
level.editor_request_begin_play()
started=time.monotonic();stopped=None;phase='setup';actors={};player=camera=manager=None;index=0;at=0.;samples=[];captured=set();seen=set()
report=dict(status='RUNNING',assisted=True,direct_input=False,loadouts=[],casts=[],autonomous=[])
film_frame=0


def attr(actor,name):
    attribute=unreal.GameplayAttribute();attribute.import_text('(Attribute="/Script/PGAbilitySystem.PGAtrributeSet:'+name+'")')
    value,valid=unreal.AbilitySystemLibrary.get_float_attribute(actor,attribute);assert valid,name
    return value


def finish(error=None):
    global stopped,actors,player,camera,manager
    if stopped is not None:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    else:unreal.log('PGP09Weapons RUNTIME PASS')
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    actors={};player=camera=manager=None;level.editor_request_end_play();stopped=time.monotonic()


def place(eid,sid):
    enemy=actors[eid];row=skills[sid]
    for oid,a in actors.items():
        a.get_controller().set_combat_thinking_enabled(False)
        a.set_actor_hidden_in_game(oid!=eid)
        parked=sorted(ids).index(oid)
        a.set_actor_location(unreal.Vector(1300+(parked%5)*180,1300+(parked//5)*180,200),False,True)
    distance=600 if row['Pattern'] in ('AimedProjectile','HazardSequence') else 220
    origin=player.get_actor_location();floor=origin.z-player.capsule_component.get_scaled_capsule_half_height()
    pos=unreal.Vector(origin.x-distance,origin.y,floor+enemy.capsule_component.get_scaled_capsule_half_height()+2)
    enemy.set_actor_location_and_rotation(pos,unreal.Rotator(),False,True)
    location=pos+unreal.Vector(280,-430,210)
    camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,pos+unreal.Vector(0,0,5)),False,True)


def capture(world,label):
    if render:assert unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/(label+'.png')).as_posix())


def tick(dt):
    global film_frame
    global phase,actors,player,camera,manager,index,at,cast_at,health_before,samples,captured,seen
    now=time.monotonic()
    if stopped is not None:
        if now-stopped>4 and placed:
            for a in placed:editor.destroy_actor(a)
            placed.clear()
        if now-stopped>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>800:finish('Timeout '+phase);return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            if now-started<8:return
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player:return
            for cmd in ('t.MaxFPS 60','PGStress 0 0','DisableAllScreenMessages','pg.AI.DebugDecisions 1','r.MotionBlurQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,cmd)
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if widget.is_in_viewport():widget.remove_from_parent()
            for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                a.get_controller().set_combat_thinking_enabled(False)
                if not a.actor_has_tag('PGP09Weapon'):continue
                eid=a.get_editor_property('character_tid');actors[eid]=a
                a.mesh.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
                visual=a.appearance_component.get_presentation_mesh();assert visual
                visual.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
                assert abs(attr(a,'MaxHealth')-definitions[eid]['stats']['Health'])<.1
                assert abs(attr(a,'AttackPower')-definitions[eid]['stats']['Attack'])<.1
                appearance=a.appearance_component.get_appearance()
                gear=[c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==visual]
                assert len(gear)==1 and gear[0].get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
                report['loadouts'].append(dict(enemy=eid,mesh=gear[0].static_mesh.get_path_name(),bounds=str(gear[0].get_local_bounds())))
                base=unreal.load_asset('/Game/DataCenter/Characters/DA_P09_'+definitions[eid]['sex'])
                assert a.appearance_component.apply_appearance(base)
                assert not [c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==a.appearance_component.get_presentation_mesh()]
                assert a.appearance_component.apply_appearance(appearance)
                assert len([c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==a.appearance_component.get_presentation_mesh()])==1
            assert set(actors)==ids
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(),unreal.Rotator())
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            manager=unreal.GameplayStatics.get_actor_of_class(world,unreal.PGStageManager);manager.start_stage(1)
            phase='navigation';at=t+25;return
        if phase=='navigation':
            if manager.get_spawned_monsters()<5:assert t<at;return
            for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                if a not in actors.values():
                    a.get_controller().set_combat_thinking_enabled(False);a.set_actor_hidden_in_game(True)
                    a.set_actor_location(unreal.Vector(2600,2400,200),False,True)
            phase='position';return
        if phase=='auto_position':
            eid,sid=auto_cases[index];place(eid,sid);at=t+1.;phase='auto_start';return
        if phase=='auto_start':
            if t<at:return
            eid,sid=auto_cases[index];actors[eid].get_controller().set_combat_thinking_enabled(True)
            at=t+55.;seen=set();phase='auto_wait';return
        if phase=='auto_wait':
            eid,sid=auto_cases[index];enemy=actors[eid]
            if sid==15759:
                # Maintain melee pressure as a chasing player would. A stationary
                # target lets the shooter retreat outside this defensive spell.
                pos=enemy.get_actor_location()+unreal.Vector(220,0,0)
                pos.z=player.get_actor_location().z
                player.set_actor_location(pos,False,True)
            bb=enemy.get_controller().get_editor_property('blackboard')
            if bb and enemy.get_editor_property('pattern_active'):seen.add(bb.get_value_as_int('SelectedSkillID'))
            if sid not in seen:assert t<at,('autonomous skill missing',eid,sid,sorted(seen));return
            enemy.get_controller().set_combat_thinking_enabled(False)
            report['autonomous'].append(dict(enemy=eid,skill=sid,seen=sorted(seen),maintained_melee_pressure=sid==15759))
            at=t+5.;phase='auto_finish';return
        if phase=='auto_finish':
            if t<at:return
            index+=1
            if index==len(auto_cases):finish();return
            phase='auto_position';return
        eid,sid=cases[index];enemy=actors[eid];row=skills[sid]
        if phase=='position':
            if t<at:return
            place(eid,sid);at=t+1.;phase='start';return
        if phase=='start':
            if t<at:return
            capture(world,str(eid)+'_'+str(sid)+'_idle')
            # Readback stalls this frame; do not charge that delta to a newly started attack.
            at=t+.3;phase='activate';return
        if phase=='activate':
            if t<at:return
            health_before=attr(player,'CurrentHealth')
            assert enemy.get_controller().try_execute_skill(sid),(eid,sid,'activation rejected')
            cast_at=t;phase='cast';samples=[];captured=set()
            report['active_cast']=dict(enemy=eid,skill=sid,samples=samples)
            return
        if phase=='cast':
            if '-PGEnemyFilm' in command:
                capture(world,'Frame'+str(film_frame).zfill(6));film_frame+=1
            visual=enemy.appearance_component.get_presentation_mesh()
            def vec(p):return [p.x,p.y,p.z]
            points={bone:vec(enemy.mesh.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT).translation) for bone in ('hand_r','hand_l')}
            gear=next(c for c in enemy.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==visual)
            hand=visual.get_socket_transform('hand_r',unreal.RelativeTransformSpace.RTS_WORLD)
            contact=unreal.MathLibrary.transform_location(hand,unreal.Vector(*SPEC['grips'][definitions[eid]['sex']]['translation']))
            error=(contact-gear.get_world_location()).length();assert error<1.,(eid,'grip contact',error)
            anim=enemy.mesh.get_anim_instance()
            montage=unreal.load_asset(row['ElitePresentationMontage'])
            samples.append(dict(time=t-cast_at,points=points,grip_error=error,
                                position=anim.montage_get_position(montage),
                                rate=anim.montage_get_play_rate(montage),
                                playing=anim.montage_is_playing(montage),
                                health=attr(player,'CurrentHealth'),yaw=enemy.get_actor_rotation().yaw))
            if '-PGEnemyTempoEvidence' in command:
                duration=row['TelegraphDuration']+row['RecoveryDuration']
                frame=int((t-cast_at)/duration*24)
                label='tempo_'+str(frame).zfill(2)
                if 0<=frame<24 and label not in captured:
                    capture(world,str(eid)+'_'+str(sid)+'_'+label);captured.add(label)
            for label,fraction in [('windup',.55),('impact',1.02)]:
                if label not in captured and t-cast_at>=row['TelegraphDuration']*fraction:
                    if label=='windup':
                        anim=enemy.mesh.get_anim_instance();montage=anim.get_current_active_montage()
                        assert montage and montage.get_path_name()==row['ElitePresentationMontage'],(eid,sid,'wrong active montage')
                        assert anim.montage_get_position(montage)>0.,(eid,sid,'frozen montage')
                    capture(world,str(eid)+'_'+str(sid)+'_'+label);captured.add(label)
            if enemy.get_editor_property('pattern_active'):return
            if row['Pattern']=='AimedProjectile' and t-cast_at<row['TelegraphDuration']+600/row['TravelSpeed']+.6:return
            damage=health_before-attr(player,'CurrentHealth');assert damage>0,(eid,sid,'no damage')
            import math
            delta=max(math.dist(s['points']['hand_r'],samples[0]['points']['hand_r']) for s in samples)
            assert delta>2.,(eid,sid,'montage clock without moving hand',delta)
            assert not enemy.get_editor_property('pattern_recovering')
            tempo_evidence=None
            tempo=row.get('MotionTempo',{})
            if tempo.get('bEnabled'):
                length=montage.get_play_length();impact=row['ImpactMontageFraction']
                anticipation=row['TelegraphDuration']-tempo['SwingSeconds']
                pose_seconds=min(tempo['AnticipationPoseSeconds'],anticipation)
                expected_rates=[length*(tempo['SwingStartFraction']-row['WindupMontageFraction'])/pose_seconds,
                                length*(impact-tempo['SwingStartFraction'])/tempo['SwingSeconds'],
                                length*(tempo['SettleStartFraction']-impact)/tempo['FollowThroughSeconds'],
                                length*(1-tempo['SettleStartFraction'])/(row['RecoveryDuration']-tempo['FollowThroughSeconds'])]
                for rate in expected_rates:
                    assert any(abs(s['rate']-rate)<.01 for s in samples),(eid,sid,'missing motion phase',rate)
                if anticipation>pose_seconds+.05:
                    held=[s for s in samples if abs(s['rate'])<.001 and abs(s['position']-length*tempo['SwingStartFraction'])<.01]
                    assert held,(eid,sid,'ready pose was not held before the swing')
                    assert all(not s['playing'] for s in held),(eid,sid,'ready hold accumulated playback time')
                    assert all(s['health']==health_before for s in held),(eid,sid,'damage during ready hold')
                first_hit=next(s['time'] for s in samples if s['health']<health_before-.01)
                assert first_hit>=row['TelegraphDuration']-.03,(eid,sid,'damage moved before warning end',first_hit)
                gaps=[b['time']-a['time'] for a,b in zip(samples,samples[1:])]
                assert first_hit<=row['TelegraphDuration']+max(gaps,default=0)+.08,(eid,sid,'late damage',first_hit)
                tempo_evidence=dict(expected_rates=expected_rates,first_hit=first_hit,warning=row['TelegraphDuration'])
            report['casts'].append(dict(enemy=eid,skill=sid,damage=damage,hand_delta=delta,samples=samples,tempo=tempo_evidence))
            report.pop('active_cast',None)
            unreal.log('PGP09Weapons CAST '+str(eid)+' '+str(sid))
            index+=1
            if index==len(cases):
                if not auto_cases:finish();return
                index=0;phase='auto_position';return
            phase='position';at=t+1.
    except Exception:finish(traceback.format_exc())


handle=unreal.register_slate_post_tick_callback(tick)

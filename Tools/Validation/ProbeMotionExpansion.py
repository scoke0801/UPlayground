"""Isolated live GAS casts, entrance locks, autonomous choices and continuous montage samples."""
import json, os, sys, time, traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureMotionExpansion import SPEC, TABLES, rows, artifact_hashes
OUT=Path(os.environ['PG_MOTION_RUN'])
assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
render='-nullrhi' not in unreal.SystemLibrary.get_command_line()
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
performance.set_editor_property('bThrottleCPUWhenNotForeground',False) # This process only; never save editor settings.
enemies={r['EnemyID']:r for r in rows(TABLES['enemies'])};skills={r['SkillID']:r for r in rows(TABLES['skills'])}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert level.load_level('/Game/Maps/RogueArena')
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
placed=[]
ids=sorted({d['enemy'] for d in SPEC['enemy_skills']+SPEC['entrances']}|{15204})
for index,eid in enumerate(ids):
    actor=editor.spawn_actor_from_class(unreal.load_class(None,enemies[eid]['ActorClass']),unreal.Vector(2000,1400+index*200,200))
    actor.set_editor_property('tags',['PGExpansion_'+str(eid)]);placed.append(actor)
del actor
level.editor_request_begin_play();started=time.monotonic();stopped=None
phase='setup';player=None;actors={};camera=None;attack=None;index=0;at=0;case=None;cast_at=0;captured=set();samples=[]
report=dict(status='RUNNING',assisted=True,direct_input=False,artifacts=artifact_hashes(),enemy_casts=[],player_casts=[],entrances=[],autonomous=[])

def hp(actor):
    attribute=unreal.GameplayAttribute();attribute.import_text('(Attribute="/Script/PGAbilitySystem.PGAtrributeSet:CurrentHealth")')
    value,valid=unreal.AbilitySystemLibrary.get_float_attribute(actor,attribute);assert valid;return value

def command(world,value):unreal.SystemLibrary.execute_console_command(world,value)

def finish(error=None):
    global stopped,player,actors,camera,attack,manager
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    else:unreal.log('PGMotionExpansion PASS')
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    player=camera=attack=manager=None;actors={};level.editor_request_end_play();stopped=time.monotonic()

def frame(world,label,center):
    if not render:return
    location=center+unreal.Vector(280,-600,370)
    camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,center),False,True)
    assert unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/(label+'.png')).as_posix())

def place(enemy,world,distance=180.):
    position=player.get_actor_location();floor=position.z-player.capsule_component.get_scaled_capsule_half_height()
    for eid,other in actors.items():
        other.get_controller().set_combat_thinking_enabled(False)
        other.set_actor_hidden_in_game(other!=enemy)
        other.set_actor_location(unreal.Vector(2000,1400+ids.index(eid)*200,200),False,True)
    enemy.set_actor_location_and_rotation(unreal.Vector(position.x-distance,position.y,floor+enemy.capsule_component.get_scaled_capsule_half_height()+3),unreal.Rotator(),False,True)

def tick(dt):
    global phase,player,actors,camera,attack,index,at,case,cast_at,captured,samples,health,origin,guard_hit,enemy_health,autonomous_seen,manager
    now=time.monotonic()
    if stopped is not None:
        if now-stopped>4 and placed:
            for a in placed:editor.destroy_actor(a)
            placed.clear()
        if now-stopped>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>350:finish('Timeout '+phase);return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            if now-started<8:return
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player:return
            command(world,'t.MaxFPS 60');command(world,'PGStress 0 0');command(world,'pg.AI.DebugDecisions 1');command(world,'pg.Skill.DebugCast 1')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if widget.is_in_viewport():widget.remove_from_parent()
            command(world,'DisableAllScreenMessages')
            actors={a.get_editor_property('character_tid'):a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if any(a.actor_has_tag('PGExpansion_'+str(eid)) for eid in ids)}
            assert set(actors)==set(ids)
            for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):a.get_controller().set_combat_thinking_enabled(False)
            attack=player.get_component_by_class(unreal.PGPlayerAttackComponent);assert attack
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(),unreal.Rotator())
            if render:unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            manager=unreal.GameplayStatics.get_actor_of_class(world,unreal.PGStageManager);assert manager
            manager.start_stage(1);at=t+25;phase='navigation_ready';return
        if phase=='navigation_ready':
            if manager.get_spawned_monsters()<5:
                assert t<at,'stage navigation initialization';return
            spawned=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if a not in actors.values()]
            for i,a in enumerate(spawned):
                a.get_controller().set_combat_thinking_enabled(False)
                a.set_actor_location(unreal.Vector(2500,1800+i*150,200),False,True)
                a.set_actor_hidden_in_game(True)
            if os.environ.get('PG_MOTION_SECTION')=='retreat':
                phase='player_prepare';index=1;return
            phase='reaction_prepare' if os.environ.get('PG_MOTION_SECTION')=='reaction' else 'entrance';index=0;return
        if phase=='entrance':
            case=SPEC['entrances'][index];eid=case['enemy']
            actors[eid].destroy_actor()
            transform=unreal.Transform(location=player.get_actor_location()+unreal.Vector(-180,0,0))
            enemy=unreal.PGEditorProbeTools.spawn_play_actor(world,unreal.load_class(None,enemies[eid]['ActorClass']),transform)
            actors[eid]=enemy;enemy.get_controller().set_combat_thinking_enabled(False);place(enemy,world)
            ready=enemy.ensure_combat_ready(player)
            assert not ready,('missing entrance lock',eid)
            if not ready:
                sid=enemies[case['enemy']]['SkillIdList'][0]
                assert not enemy.get_controller().try_execute_skill(sid),'entrance allowed attack'
            report['entrances'].append(dict(enemy=case['enemy'],observed_lock=not ready))
            at=t+case['duration']+.3;phase='entrance_wait';return
        if phase=='entrance_wait':
            if t<at:return
            assert actors[case['enemy']].ensure_combat_ready(player)
            frame(world,'entrance_'+str(case['enemy']),actors[case['enemy']].get_actor_location())
            index+=1
            if index<len(SPEC['entrances']):phase='entrance'
            else:index=0;phase='enemy_prepare'
            return
        if phase=='enemy_prepare':
            case=SPEC['enemy_skills'][index];enemy=actors[case['enemy']]
            place(enemy,world,500 if skills[case['id']]['Pattern'] in ('Summon','AimedProjectile') else 180)
            health=hp(player);at=t+.6;phase='enemy_cast';return
        if phase=='enemy_cast':
            if t<at:return
            enemy=actors[case['enemy']]
            assert enemy.get_controller().try_execute_skill(case['id']),('activation',case['id'])
            cast_at=t;samples=[];captured=set();phase='enemy_wait';return
        if phase=='enemy_wait':
            enemy=actors[case['enemy']];row=skills[case['id']];anim=enemy.mesh.get_anim_instance();am=anim.get_current_active_montage()
            if am:samples.append([round(t-cast_at,4),round(anim.montage_get_position(am),4)])
            for mark in [.2,.55,.85,1.]:
                if mark not in captured and t-cast_at>=row['TelegraphDuration']*mark:
                    frame(world,'enemy_'+str(case['id'])+'_'+str(mark),enemy.get_actor_location());captured.add(mark)
            if enemy.get_editor_property('pattern_active') or t-cast_at<row['TelegraphDuration']+row['RecoveryDuration']+1:return
            assert len(samples)>4,('motion samples',case['id'])
            if row['Pattern']=='Summon':
                children=list(enemy.get_living_pattern_summons())
                assert len(children)==2,('summons',[(a.get_name(),str(a.get_owner())) for a in children])
                for child in children:child.get_controller().set_combat_thinking_enabled(False);child.destroy_actor()
            else:assert hp(player)<health,('no damage',case['id'])
            report['enemy_casts'].append(dict(skill=case['id'],damage=health-hp(player),samples=samples))
            index+=1
            if index<len(SPEC['enemy_skills']):phase='enemy_prepare'
            else:index=0;phase='player_prepare'
            return
        if phase=='player_prepare':
            case=SPEC['player'][min(index,2)];enemy=actors[15304];place(enemy,world,700)
            origin=player.get_actor_location();enemy_health=hp(enemy);health=hp(player)
            command(world,'PGMotionExpansionCast '+str(case['id']))
            assert attack.is_running(),('player activation',case['id'])
            forward=player.get_actor_forward_vector();distance=480 if case['id']==116 else 190
            point=origin+forward*distance;point.z=origin.z-player.capsule_component.get_scaled_capsule_half_height()+enemy.capsule_component.get_scaled_capsule_half_height()+3
            enemy.set_actor_location(point,False,True);guard_hit=False;samples=[];captured=set();cast_at=t;phase='player_wait';return
        if phase=='player_wait':
            clock=attack.get_logical_time()
            if attack.is_running():
                hand=player.mesh.get_socket_location('hand_r')-player.get_actor_location()
                samples.append([round(t-cast_at,4),round(clock,4),hand.x,hand.y,hand.z])
                if case['id']==117 and clock>=.45:
                    pelvis=player.mesh.get_socket_location('pelvis')-player.get_actor_location()
                    offset=pelvis.dot(player.get_actor_forward_vector())
                    report.setdefault('retreat_root_samples',[]).append(dict(clock=clock,pelvis_forward=offset,
                        travel=(player.get_actor_location()-origin).dot(player.get_actor_forward_vector())))
                    assert abs(offset)<35.,('retreat mesh left capsule',clock,offset)
            if case['id']==118 and index==2 and not guard_hit and .15<=clock<.6:
                command(world,'PGMotionExpansionHit');guard_hit=True
                assert hp(player)==health,'counter did not block direct hit'
            for mark in ([.15,case['hit'],.6,.85] if case['id']==117 else [.15,case['hit'],min(case['duration']-.05,case['hit']+.2)]):
                if mark not in captured and clock>=mark:
                    frame(world,'player_'+str(case['id'])+'_'+str(index)+'_'+str(mark),player.get_actor_location());captured.add(mark)
            if attack.is_running():return
            enemy=actors[15304]
            if index==3:assert hp(enemy)==enemy_health,'failed guard dealt damage'
            else:assert hp(enemy)<enemy_health,('player no damage',case['id'])
            travel=player.get_actor_location()-origin
            poses=[unreal.Vector(*sample[2:]) for sample in samples if sample[1]>.12]
            assert poses and max((p-poses[0]).length() for p in poses)>8.,('hand pose did not animate',case['id'])
            if case['id']==117:
                assert abs(travel.dot(player.get_actor_forward_vector())+220.)<5.,('incorrect root travel',travel)
                assert len(report.get('retreat_root_samples',[]))>=3,'missing retreat pose samples'
            report['player_casts'].append(dict(skill=case['id'],counter_success=guard_hit,damage=enemy_health-hp(enemy),travel=[travel.x,travel.y,travel.z],samples=samples))
            if os.environ.get('PG_MOTION_SECTION')=='retreat':finish();return
            index+=1
            if index<4:phase='player_prepare'
            else:phase='autonomous_prepare';index=0
            return
        if phase=='autonomous_prepare':
            case=SPEC['enemy_skills'][index];enemy=actors[case['enemy']];place(enemy,world,500 if skills[case['id']]['Pattern'] in ('Summon','AimedProjectile') else 220)
            enemy.get_controller().set_combat_thinking_enabled(True);at=t+40;autonomous_seen=set();phase='autonomous_wait';return
        if phase=='autonomous_wait':
            enemy=actors[case['enemy']]
            # Blackboard contains the committed skill throughout its GAS lifetime.
            blackboard=enemy.get_controller().get_editor_property('blackboard')
            if blackboard and enemy.get_editor_property('pattern_active'):autonomous_seen.add(blackboard.get_value_as_int('SelectedSkillID'))
            if case['id'] not in autonomous_seen:
                assert t<at,('new skill not autonomously selected',case['id'],list(autonomous_seen));return
            enemy.get_controller().set_combat_thinking_enabled(False)
            report['autonomous'].append(dict(skill=case['id'],selected=sorted(autonomous_seen)))
            index+=1
            if index<len(SPEC['enemy_skills']):phase='autonomous_prepare'
            else:phase='reaction_prepare'
            return
        if phase=='reaction_prepare':
            enemy=actors[15204];place(enemy,world,200)
            at=t+3;phase='reaction_cast';return
        if phase=='reaction_cast':
            enemy=actors[15204]
            if not enemy.get_controller().try_execute_skill(enemies[15204]['SkillIdList'][0]):
                assert t<at,'reaction setup attack admission';return
            phase='reaction_attack';return
        if phase=='reaction_attack':
            enemy=actors[15204]
            if enemy.get_editor_property('pattern_active'):return
            at=t+.8;phase='reaction_begin';return
        if phase=='reaction_begin':
            if t<at:return
            enemy=actors[15204]
            command(world,'PGMotionExpansionHoldAttacks 15204')
            place(enemy,world,300)
            enemy.get_controller().set_combat_thinking_enabled(True)
            command(world,'PGMotionExpansionCast 118');assert attack.is_running()
            at=t+2;origin=enemy.get_actor_location();phase='reaction_wait';return
        if phase=='reaction_wait':
            if t<at:return
            enemy=actors[15204];count=enemy.get_controller().get_reaction_move_count()
            assert count==1,('reaction move',count)
            report['reaction']=dict(count=count,travel=(enemy.get_actor_location()-origin).length())
            command(world,'PGMotionExpansionHoldAttacks 15204');command(world,'PGMotionExpansionCast 118')
            at=t+2;phase='reaction_repeat';return
        if phase=='reaction_repeat':
            if t<at:return
            enemy=actors[15204]
            assert enemy.get_controller().get_reaction_move_count()==1,'repeated evasions denied'
            enemy.get_controller().set_combat_thinking_enabled(False)
            report['reaction']['repeat_blocked']=True
            finish()
    except Exception:finish(traceback.format_exc())

in_tick=False
def guarded_tick(dt):
    global in_tick
    if in_tick:return
    in_tick=True
    try:tick(dt)
    finally:in_tick=False
handle=unreal.register_slate_post_tick_callback(guarded_tick)

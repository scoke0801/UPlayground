"""Live PIE: authored attacks, mobility, capped summons and real stage spawn queues."""
import json,os,sys,time,traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureCreatureCombat import SPEC,TABLES,rows,validate
validate()
OUT=Path(os.environ['PG_CREATURE_RUN'])
assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
render='-nullrhi' not in unreal.SystemLibrary.get_command_line()
enemies={r['EnemyID']:r for r in rows(TABLES['enemies'])};skills={r['SkillID']:r for r in rows(TABLES['skills'])}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert level.load_level('/Game/Maps/RogueArena')
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);placed=[]
for i,d in enumerate(SPEC['monsters']):
    a=editor.spawn_actor_from_class(unreal.load_class(None,enemies[d['id']]['ActorClass']),unreal.Vector(900+i*300,1400,250))
    a.set_editor_property('tags',['PGCreatureProbe']);placed.append(a)
camera_placed=editor.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,0,800))
camera_placed.set_editor_property('tags',['PGCreatureCamera']);placed.append(camera_placed)
level.editor_request_begin_play();started=time.monotonic();stopped=None
phase='setup';actors={};player=None;index=0;at=0;stage_index=0
cases=[(d['id'],sid) for d in SPEC['monsters'] for sid in d['skills']]
report=dict(status='RUNNING',assisted=True,casts=[],mobility=[],stages=[])

def hp(actor):
    a=unreal.GameplayAttribute();a.import_text('(Attribute="/Script/PGAbilitySystem.PGAtrributeSet:CurrentHealth")')
    value,found=unreal.AbilitySystemLibrary.get_float_attribute(actor,a);assert found;return value

def children(world):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if a!=actors[15502] and a.get_instigator()==actors[15502]]

def finish(error=None):
    global stopped,actors,player,camera,manager,child_refs
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    else:unreal.log('PGCreatureProbe PASS')
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    actors={};player=camera=manager=None;child_refs=[];level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global phase,actors,player,index,at,health,cast_at,origin,stage_index,manager,summon_before,child_refs,camera
    now=time.monotonic()
    if stopped is not None:
        if now-stopped>4 and placed:
            for a in placed:editor.destroy_actor(a)
            placed.clear()
        if now-stopped>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>260:finish('Timeout '+phase);return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            if now-started<8:return
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player:return
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world,'PGStress 0 0')
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if a.actor_has_tag('PGCreatureCamera'))
            if render:
                unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
                for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                    if widget.is_in_viewport():widget.remove_from_parent()
                unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                a.get_controller().set_combat_thinking_enabled(False)
                if a.actor_has_tag('PGCreatureProbe'):
                    actors[a.get_editor_property('character_tid')]=a
                    a.mesh.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
            assert set(actors)=={15501,15502,15503,15504}
            for eid,mode in [(15501,unreal.MovementMode.MOVE_FLYING),(15502,unreal.MovementMode.MOVE_NONE),(15503,unreal.MovementMode.MOVE_NONE),(15504,unreal.MovementMode.MOVE_WALKING)]:
                assert actors[eid].character_movement.get_editor_property('movement_mode')==mode,(eid,'movement mode')
            manager=unreal.GameplayStatics.get_actor_of_class(world,unreal.PGStageManager);assert manager
            manager.start_stage(1)
            phase='navigation_ready';at=t+25;return
        if phase=='navigation_ready':
            spawned=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if not a.actor_has_tag('PGCreatureProbe')]
            if manager.get_spawned_monsters()<5:
                assert t<at,'Stage navigation initialization';return
            for i,a in enumerate(spawned):
                a.get_controller().set_combat_thinking_enabled(False)
                a.set_actor_location(unreal.Vector(2500,1800+i*150,200),False,True)
                a.set_actor_hidden_in_game(True)
            phase='position';at=t;return
        if phase=='position':
            if t<at:return
            eid,sid=cases[index];enemy=actors[eid];row=skills[sid]
            for other in actors.values():
                other.get_controller().set_combat_thinking_enabled(False)
                other.set_actor_hidden_in_game(other!=enemy)
                other.set_actor_location(unreal.Vector(1400,1400+(other.get_editor_property('character_tid')%4)*300,200),False,True)
            for child in children(world):
                child.get_controller().set_combat_thinking_enabled(False)
                child.set_actor_hidden_in_game(True)
                child.set_actor_location(unreal.Vector(2400,2200,100),False,True)
            pos=player.get_actor_location();floor=pos.z-player.capsule_component.get_scaled_capsule_half_height()
            distance=500 if row['Pattern'] in ('AimedProjectile','Summon') else 160
            if row['Pattern']=='RingBurst':distance=(row['InnerSafeRadius']+row['TelegraphRadius'])*.5
            maximum=row['SkillRange'] or 10000.
            if row['Pattern'] in ('Sweep','RingBurst'):maximum=min(maximum,row['TelegraphRadius'])
            if row['Pattern'] in ('AimedProjectile','Thrust'):maximum=min(maximum,row['TravelDistance'])
            distance=max(row['MinimumActivationRange']+10.,min(distance,maximum-5.))
            half=enemy.capsule_component.get_scaled_capsule_half_height()
            enemy.set_actor_location_and_rotation(unreal.Vector(pos.x-distance,pos.y,floor+half+3),unreal.Rotator(),False,True)
            center=enemy.get_actor_location();view=center+unreal.Vector(220,-620,310)
            camera.set_actor_location_and_rotation(view,unreal.MathLibrary.find_look_at_rotation(view,center),False,True)
            health=hp(player);summon_before=len(children(world));phase='cast';at=t+1;return
        if phase=='cast':
            if t<at:return
            eid,sid=cases[index];assert actors[eid].get_controller().try_execute_skill(sid),(eid,sid,'activation')
            cast_at=t;phase='windup';return
        if phase=='windup':
            eid,sid=cases[index];enemy=actors[eid];row=skills[sid]
            if t-cast_at<row['TelegraphDuration']*.5:return
            am=enemy.mesh.get_anim_instance().get_current_active_montage();assert am and am.get_path_name()==row['ElitePresentationMontage'],(eid,sid,'montage')
            assert enemy.mesh.get_anim_instance().montage_get_position(am)>0
            if render:unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+(OUT/(str(eid)+'_'+str(sid)+'.png')).as_posix()+'"')
            phase='cast_done';return
        if phase=='cast_done':
            eid,sid=cases[index];enemy=actors[eid];row=skills[sid]
            if enemy.get_editor_property('pattern_active') or t-cast_at<row['TelegraphDuration']+row['RecoveryDuration']+1:return
            if row['Pattern']=='Summon':
                spawned=children(world);assert len(spawned)==summon_before+2,('summon count',len(spawned))
                for child in spawned:
                    assert child.get_editor_property('character_tid')==15504,(child.get_path_name(),child.get_editor_property('character_tid'),str(child.get_instigator()))
                    assert child.get_controller() and hp(child)>0
                    child.get_controller().set_combat_thinking_enabled(False)
            else:assert hp(player)<health,(eid,sid,'no damage',health,hp(player))
            report['casts'].append(dict(enemy=eid,skill=sid,damage=health-hp(player)))
            index+=1
            if index<len(cases):phase='position';at=t+.3
            else:phase='mobility_start'
            return
        if phase=='mobility_start':
            origin={};pos=player.get_actor_location();floor=pos.z-player.capsule_component.get_scaled_capsule_half_height()
            for i,(eid,enemy) in enumerate(actors.items()):
                enemy.set_actor_hidden_in_game(False)
                half=enemy.capsule_component.get_scaled_capsule_half_height()
                enemy.set_actor_location(unreal.Vector(pos.x-800,pos.y+(i-1)*150,floor+half+3),False,True)
                origin[eid]=enemy.get_actor_location();enemy.get_controller().set_combat_thinking_enabled(True)
            phase='mobility_done';at=t+2;return
        if phase=='mobility_done':
            if t<at:return
            for eid,enemy in actors.items():
                enemy.get_controller().set_combat_thinking_enabled(False)
                delta=(enemy.get_actor_location()-origin[eid]).length()
                assert delta<2 if eid in (15502,15503) else delta>70,(eid,'movement',delta)
                if eid==15501:
                    assert enemy.get_actor_location().z>origin[eid].z+20,'flying must gain altitude'
                    if render:
                        center=enemy.get_actor_location();view=center+unreal.Vector(220,-620,310)
                        camera.set_actor_location_and_rotation(view,unreal.MathLibrary.find_look_at_rotation(view,center),False,True)
                        unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+(OUT/'15501_flight.png').as_posix()+'"')
                report['mobility'].append(dict(enemy=eid,travel=delta,height_change=enemy.get_actor_location().z-origin[eid].z))
            phase='summon_cap';at=t+13;return
        if phase=='summon_cap':
            if t<at:return
            plant=actors[15502]
            if len(children(world))<4:assert plant.get_controller().try_execute_skill(15523),'second summon activation'
            phase='summon_cap_done';at=t+3;return
        if phase=='summon_cap_done':
            if t<at:return
            child_refs=children(world);assert len(child_refs)==4,('cap',len(child_refs))
            for child in child_refs:child.get_controller().set_combat_thinking_enabled(False)
            phase='summon_reject';at=t+13;return
        if phase=='summon_reject':
            if t<at:return
            assert not actors[15502].get_controller().try_execute_skill(15523),'cap must reject after cooldown'
            report['summon_cap']=len(child_refs)
            child_refs[0].destroy_actor()
            phase='summon_cancel';at=t+.5;return
        if phase=='summon_cancel':
            if t<at:return
            assert actors[15502].get_controller().try_execute_skill(15523),'freed summon slot must be reusable'
            actors[15502].get_controller().set_combat_thinking_enabled(False)
            phase='summon_cancel_done';at=t+2;return
        if phase=='summon_cancel_done':
            if t<at:return
            assert len(children(world))==3,'cancelled windup must not spawn later'
            report['cancelled_summon_no_spawn']=True
            actors[15502].destroy_actor();phase='cleanup';at=t+.5;return
        if phase=='cleanup':
            if t<at:return
            live=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy)
            assert not any(c in live for c in child_refs),'orphan summons'
            manager=unreal.GameplayStatics.get_actor_of_class(world,unreal.PGStageManager);assert manager
            phase='stage_start';return
        if phase=='stage_start':
            manager.start_stage(stage_index+1);phase='stage_wait';at=t+15;return
        if phase=='stage_wait':
            target=[15504,15503,15501,15502][stage_index]
            spawned=[]
            for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                if a.get_controller():a.get_controller().set_combat_thinking_enabled(False)
                if not a.actor_has_tag('PGCreatureProbe'):spawned.append(a.get_editor_property('character_tid'))
            if target not in spawned:
                assert t<at,('stage spawn',stage_index+1,target,spawned);return
            report['stages'].append(dict(stage=stage_index+1,enemies=spawned));stage_index+=1
            if stage_index==4:finish()
            else:phase='stage_start'
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)

"""Real GAS/AI/phase/death checks in an assisted, isolated boss encounter."""
import json,os,re,time,traceback
from pathlib import Path
import unreal

OUT=Path(os.environ['PG_DARK_KNIGHT_RUN'])
STAGE_ROSTER='-PGDarkKnightStageRoster' in unreal.SystemLibrary.get_command_line()
ENCOUNTER='PGStartStage 6' if STAGE_ROSTER else 'PGBossEncounter 15601'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
assert unreal.PGEditorProbeTools.begin_play_window()
started=time.monotonic();stopped=None;phase='setup';at=0.;index=0
boss=player=None;origin=None;cast_at=0.;health_before=0.;locked=None
cases=[('sweep',15611,250,False),('uppercut',15612,300,False),('double',15613,250,False),
       ('uppercut_side_safe',15612,300,True),('fury',15614,220,False)]
report=dict(status='RUNNING',assisted=True,direct_input=False,saved_stage_roster=STAGE_ROSTER,cases=[],samples=[])

def console(world,text):unreal.SystemLibrary.execute_console_command(world,text)
def attr(actor,name):
    x=unreal.GameplayAttribute();x.import_text('(Attribute="/Script/PGAbilitySystem.PGAtrributeSet:'+name+'")')
    value,found=unreal.AbilitySystemLibrary.get_float_attribute(actor,x);assert found;return value
def position(distance,side=False):
    player.set_actor_location(origin,False,True)
    z=origin.z-player.capsule_component.get_scaled_capsule_half_height()+boss.capsule_component.get_scaled_capsule_half_height()
    boss.set_actor_location_and_rotation(unreal.Vector(origin.x-distance,origin.y,z),unreal.Rotator(),False,True)
def capture(world,label):
    assert unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/(label+'.png')).as_posix()),label
    report['samples'].append(dict(label=label,phase=boss.get_editor_property('boss_phase'),health=attr(player,'CurrentHealth')))
def finish(error=None):
    global stopped
    if stopped is not None:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    else:unreal.log('PGDarkKnight runtime PASS')
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global boss,player,origin,phase,at,index,cast_at,health_before,locked
    now=time.monotonic();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and now-stopped>4:
            boss=player=None
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>300:finish('Timeout '+phase);return
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or now-started<5:return
            console(world,'t.MaxFPS 60');console(world,'PGStress 0 0');console(world,ENCOUNTER)
            console(world,'DisableAllScreenMessages');origin=player.get_actor_location();phase='find';at=t+1;return
        if t<at:return
        if phase=='find':
            found=[e for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if e.get_editor_property('character_tid')==15601]
            if not found:return
            boss=found[0];boss.get_controller().set_combat_thinking_enabled(False)
            for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True):
                if not isinstance(w,unreal.PGUIMainHUD):w.remove_from_parent()
            position(250)
            assert not boss.get_controller().try_execute_skill(15614),'Phase-two skill opened phase one'
            assert not boss.get_controller().try_execute_skill(15401),'Unowned skill was accepted'
            phase='cancel_start';return
        controller=boss.get_controller() if phase not in ('results','restart') else None
        if phase=='cancel_start':
            if not controller.try_execute_skill(15611):return
            health_before=attr(player,'CurrentHealth');controller.set_combat_thinking_enabled(False)
            assert not boss.get_editor_property('pattern_active')
            phase='cancel_check';at=t+1.6;return
        if phase=='cancel_check':
            assert attr(player,'CurrentHealth')==health_before,'Cancelled attack dealt delayed damage'
            report['cancel_cleanup']=True;phase='start';return
        label,sid,distance,side=cases[index] if index<len(cases) else ('',0,0,False)
        if phase=='start':
            position(distance)
            if label=='fury' and boss.get_editor_property('boss_phase')==1:
                if not controller.try_execute_skill(15611):return
                console(world,'PGBossDamage '+str(attr(boss,'CurrentHealth')-attr(boss,'MaxHealth')*.49))
                assert boss.is_boss_transitioning() and not boss.get_editor_property('pattern_active')
                report['phase_cancel']=True;capture(world,'phase_transition');at=t+1.7;return
            if not controller.try_execute_skill(sid):return
            cast_at=t;health_before=attr(player,'CurrentHealth');locked=boss.get_actor_forward_vector();phase='windup';return
        if phase=='windup':
            if t-cast_at<.35:return
            capture(world,label+'_windup')
            montage=boss.mesh.get_anim_instance().get_current_active_montage();assert montage
            assert montage.get_path_name().startswith('/Game/DataCenter/DarkKnightBoss/'),montage
            if side:
                p=boss.get_actor_location();player.set_actor_location(unreal.Vector(p.x,p.y+distance,origin.z),False,True)
            phase='recover';return
        if phase=='recover':
            if not boss.get_editor_property('pattern_recovering'):return
            damage=health_before-attr(player,'CurrentHealth')
            assert damage==0 if side else damage>0,(label,damage)
            assert (boss.get_actor_forward_vector()-locked).length()<.05,'Aim lock changed'
            capture(world,label+'_recovery');phase='end';return
        if phase=='end':
            if boss.get_editor_property('pattern_active'):return
            assert not controller.try_execute_skill(15612),'Minimum recovery wait skipped'
            report['cases'].append(dict(label=label,skill=sid,damage=health_before-attr(player,'CurrentHealth'),elapsed=t-cast_at))
            index+=1
            phase='start' if index<len(cases) else 'death_start';at=t+.65;return
        if phase=='death_start':
            position(250)
            if not controller.try_execute_skill(15611):return
            console(world,'PGBossDamage 1000000');assert not boss.get_editor_property('pattern_active')
            capture(world,'defeated');phase='results';at=t+4;return
        if phase=='results':
            assert any(isinstance(w,unreal.PGUIWindowRewardSelect) and w.is_in_viewport() for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True)),'Victory results missing'
            report['death_and_results']=True;console(world,ENCOUNTER);phase='restart';at=t+1;return
        if phase=='restart':
            found=[e for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if e.get_editor_property('character_tid')==15601 and e!=boss]
            if not found:return
            boss=found[0];assert boss.get_editor_property('boss_phase')==1
            position(250);console(world,'PGBossDamage '+str(attr(boss,'CurrentHealth')-attr(boss,'MaxHealth')*.49))
            boss.get_controller().set_combat_thinking_enabled(True)
            unreal.log('PGDarkKnight AUTONOMOUS BEGIN');phase='autonomous';cast_at=t;return
        if phase=='autonomous':
            if t-cast_at<36:return
            log=(OUT/'runtime.log').read_text(encoding='utf-8-sig',errors='replace').split('PGDarkKnight AUTONOMOUS BEGIN')[-1]
            report['autonomous_skills']=sorted(set(map(int,re.findall(r'PGPattern Windup skill=(156\d+)',log))))
            assert {15611,15612,15613,15614}<=set(report['autonomous_skills']),report['autonomous_skills']
            capture(world,'autonomous');finish()
    except Exception:finish(traceback.format_exc())

in_tick=False
def guarded_tick(dt):
    global in_tick
    if in_tick:return
    in_tick=True
    try:tick(dt)
    finally:in_tick=False
handle=unreal.register_slate_post_tick_callback(guarded_tick)

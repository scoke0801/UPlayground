"""Real stage/GAS boss probe. Assisted, isolated profile; no gameplay assets are saved."""
import json
from pathlib import Path
import re
import time
import traceback
import unreal

command=unreal.SystemLibrary.get_command_line();assert '-PGTestProfile=' in command
OUT=Path(re.search(r'-PGHumanoidEvidence="?([^"\s]+)',command)[1]);render='-nullrhi' not in command
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert level.load_level('/Game/Maps/RogueArena')
assert unreal.PGEditorProbeTools.begin_play_window()
started=time.monotonic();stopped=None;phase='setup';at=0.;index=0
player=boss=None;origin=None;cast_at=0.;health_before=0.;locked=None
report=dict(status='RUNNING',assisted=True,direct_input=False,cases=[],samples=[])
cases=[('combo1',15401,250),('thrust',15402,400),('guard_success',15404,250),('charge',15403,700),
       ('combo2',15401,250),('ring_safe',15405,160),('ring_hit',15405,450),('guard_fail',15404,250)]

def attr(actor,name):
    x=unreal.GameplayAttribute();x.import_text('(Attribute="/Script/PGAbilitySystem.PGAtrributeSet:'+name+'")')
    value,found=unreal.AbilitySystemLibrary.get_float_attribute(actor,x);assert found;return value

def console(world,text):unreal.SystemLibrary.execute_console_command(world,text)

def capture(world,label):
    report['samples'].append(dict(label=label,phase=boss.get_editor_property('boss_phase'),active=boss.get_editor_property('pattern_active'),
        recovering=boss.get_editor_property('pattern_recovering'),guarding=boss.get_editor_property('guarding'),health=attr(player,'CurrentHealth')))
    if render:assert unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/(label+'.png')).as_posix()),'Empty game capture: '+label

def position(distance):
    player.set_actor_location(origin,False,True)
    half=boss.capsule_component.get_scaled_capsule_half_height()
    ground=origin.z-player.capsule_component.get_scaled_capsule_half_height()
    boss.set_actor_location_and_rotation(unreal.Vector(origin.x-distance,origin.y,ground+half),unreal.Rotator(),False,True)

def finish(error=None):
    global stopped
    if stopped is not None:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error('PGHumanoidProbe FAIL '+error)
    else:unreal.log('PGHumanoidProbe PASS')
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global player,boss,origin,phase,at,index,cast_at,health_before,locked
    now=time.monotonic();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and now-stopped>4:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>230:finish('Timeout '+phase);return
    if not world:return
    t=unreal.GameplayStatics.get_time_seconds(world)
    try:
        if phase=='setup':
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or now-started<5:return
            console(world,'t.MaxFPS 60');console(world,'PGStress 0 0');console(world,'PGBossEncounter 15401')
            console(world,'DisableAllScreenMessages')
            origin=player.get_actor_location();phase='find';at=t+1;return
        if t<at:return
        if phase=='find':
            found=[e for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if e.get_editor_property('character_tid')==15401]
            if not found:return
            boss=found[0];boss.get_controller().set_combat_thinking_enabled(False)
            for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True):
                if not isinstance(w,unreal.PGUIMainHUD):w.remove_from_parent()
            position(250)
            assert not boss.get_controller().try_execute_skill(15404),'Guard cannot open the fight'
            assert not boss.get_controller().try_execute_skill(15405),'Phase one cannot use ring'
            phase='start';return
        label,sid,distance=cases[index] if index<len(cases) else ('',0,0)
        controller=boss.get_controller() if phase not in ('results','restart') else None
        if phase=='start':
            position(distance)
            if label=='combo2' and boss.get_editor_property('boss_phase')==1:
                if not controller.try_execute_skill(15401):return
                console(world,'PGBossDamage '+str(attr(boss,'CurrentHealth')-attr(boss,'MaxHealth')*.49))
                assert boss.is_boss_transitioning() and not boss.get_editor_property('pattern_active') and not boss.get_editor_property('guarding')
                capture(world,'transition');at=t+1.4;return
            if not controller.try_execute_skill(sid):return # Real cooldown/minimum wait remain authoritative.
            cast_at=t;health_before=attr(player,'CurrentHealth');locked=boss.get_actor_forward_vector();phase='windup';return
        if phase=='windup':
            if t-cast_at<.2:return
            capture(world,label+'_windup')
            montage=boss.mesh.get_anim_instance().get_current_active_montage();assert montage,'Missing authored montage'
            report.setdefault('montages',[]).append(montage.get_path_name())
            if label=='guard_success':phase='guard_hold'
            elif label=='guard_fail':phase='guard_expire'
            else:phase='recover'
            return
        if phase=='guard_hold':
            if not boss.get_editor_property('guarding'):return
            capture(world,'guard_hold')
            # Rear direct contact and external/DoT-style GAS damage cannot accept the guard.
            front=player.get_actor_location();p=boss.get_actor_location();player.set_actor_location(unreal.Vector(p.x-200,p.y,front.z),False,True)
            console(world,'PGBossDirectHit');assert boss.get_editor_property('guarding'),'Rear damage triggered success'
            console(world,'PGBossDamage 1');assert boss.get_editor_property('guarding'),'Proc damage triggered success'
            player.set_actor_location(front,False,True)
            console(world,'PGBossDirectHit');assert not boss.get_editor_property('guarding'),'Front direct hit did not accept guard'
            console(world,'PGBossDirectHit') # Further multihit must see ordinary defense.
            phase='counter_wait';at=t+.45;return
        if phase=='counter_wait':
            assert attr(player,'CurrentHealth')==health_before,'Counter struck without full warning'
            capture(world,'counter_warning');phase='recover';return
        if phase=='guard_expire':
            if not boss.get_editor_property('pattern_recovering'):return
            assert not boss.get_editor_property('guarding');assert attr(player,'CurrentHealth')==health_before
            capture(world,'guard_failed_recovery');phase='end';return
        if phase=='recover':
            if not boss.get_editor_property('pattern_recovering'):return
            capture(world,label+'_recovery')
            if label=='ring_safe':assert attr(player,'CurrentHealth')==health_before,'Ring center was not safe'
            else:assert attr(player,'CurrentHealth')<health_before,(label,'No GAS damage')
            if label in ('combo1','combo2','thrust'):
                assert (boss.get_actor_forward_vector()-locked).length()<.05,'Locked direction changed'
            phase='end';return
        if phase=='end':
            if boss.get_editor_property('pattern_active'):return
            assert not controller.try_execute_skill(15402),'Post-recovery wait was skipped'
            report['cases'].append(dict(label=label,skill=sid,damage=health_before-attr(player,'CurrentHealth'),elapsed=t-cast_at))
            index+=1
            if index<len(cases):phase='start';at=t+.6;return
            phase='death_start';at=t+.6;return
        if phase=='death_start':
            position(250)
            if not controller.try_execute_skill(15401):return
            console(world,'PGBossDamage 1000000')
            assert not boss.get_editor_property('pattern_active') and not boss.get_editor_property('guarding')
            capture(world,'defeated');phase='results';at=t+4;return
        if phase=='results':
            assert any(isinstance(w,unreal.PGUIWindowRewardSelect) and w.is_in_viewport() for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True)),'Missing victory results'
            console(world,'PGProfileStatus');console(world,'PGBossEncounter 15401');phase='restart';at=t+1;return
        if phase=='restart':
            fresh=[e for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy) if e.get_editor_property('character_tid')==15401 and e!=boss]
            if not fresh:return
            boss=fresh[0];boss.get_controller().set_combat_thinking_enabled(False)
            assert boss.get_editor_property('boss_phase')==1 and not boss.get_editor_property('guarding')
            capture(world,'restart');position(250)
            unreal.log('PGHumanoidProbe Autonomous BEGIN')
            console(world,'PGBossDamage '+str(attr(boss,'CurrentHealth')-attr(boss,'MaxHealth')*.49))
            boss.get_controller().set_combat_thinking_enabled(True)
            report['autonomous_skills']=[];cast_at=t;phase='autonomous';at=t+1.4;return
        if phase=='autonomous':
            if t-cast_at<25:return
            log=(OUT/'runtime.log').read_text(encoding='utf-8-sig',errors='replace').split('PGHumanoidProbe Autonomous BEGIN')[-1]
            report['autonomous_skills']=sorted(set(map(int,re.findall(r'PGPattern Windup skill=(154\d+)',log))))
            assert {15401,15404,15405}.issubset(report['autonomous_skills']),report['autonomous_skills']
            capture(world,'autonomous');finish()
    except Exception:finish(traceback.format_exc())

# Slate screenshot/PIE requests can pump another tick before this one returns.
# Keep each probe transition atomic, including synchronous console commands.
in_tick=False
def guarded_tick(dt):
    global in_tick
    if in_tick:return
    in_tick=True
    try:tick(dt)
    finally:in_tick=False

handle=unreal.register_slate_post_tick_callback(guarded_tick)

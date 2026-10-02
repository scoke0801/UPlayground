"""Rendered stage-six boss probe using real abilities and GAS damage. No assets are saved."""
import json
import time
import unreal

assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
level.editor_request_begin_play()
started = time.monotonic()
stop_requested = None
initialized = False
player = boss = None
origin = None
elapsed = at = 0.
state = 'setup'
skill_index = 0
shots = 0
skills = (15106, 15107, 15108)
samples = []


def finish():
    global stop_requested
    if stop_requested is None:
        level.editor_request_end_play()
        stop_requested = time.monotonic()


def sample(label):
    global shots
    if label == 'results':
        row = dict(next(r for r in samples if r['state'] == 'defeated'), state=label)
    else:
        row = dict(state=label, phase=boss.get_editor_property('boss_phase'),
                   active=boss.get_editor_property('pattern_active'),
                   recovering=boss.get_editor_property('pattern_recovering'),
                   transitioning=boss.is_boss_transitioning(), position=str(boss.get_actor_location()))
    samples.append(row)
    unreal.log('PGBossProbe sample=' + json.dumps(row))
    unreal.SystemLibrary.execute_console_command(player, 'PGCombatStats')
    unreal.SystemLibrary.execute_console_command(player, 'Shot SHOWUI')
    shots += 1


def position(distance):
    player.set_actor_location(origin, False, True)
    boss.set_actor_location_and_rotation(unreal.Vector(origin.x-distance, origin.y, origin.z), unreal.Rotator(), False, True)


def tick(dt):
    global initialized, player, boss, origin, elapsed, at, state, skill_index
    now = time.monotonic()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stop_requested is not None:
        if not world and now-stop_requested > 4:
            player = boss = None
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now-started > 150:
        unreal.log_error('PGBossProbe TIMEOUT state=' + state)
        finish()
        return
    if not world:
        return
    elapsed += dt
    try:
        if not initialized:
            player = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not player or elapsed < 2:
                return
            unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world, 'PGStress 0 0')
            unreal.SystemLibrary.execute_console_command(world, 'PGStartStage 6')
            origin = player.get_actor_location()
            initialized = True
            at = elapsed
            return
        if elapsed < at:
            return
        if state in ('setup', 'restart'):
            found = [e for e in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGCharacterEnemy)
                     if e.get_editor_property('character_tid') == 15106 and (state != 'restart' or e != boss)]
            if not found:
                return
            boss = found[0]
            boss.get_controller().set_combat_thinking_enabled(False)
            assert boss.get_editor_property('boss_phase') == 1
            assert not boss.get_controller().try_execute_skill(15108), 'Phase-one wave was allowed'
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, True):
                if not isinstance(widget, unreal.PGUIMainHUD):
                    widget.remove_from_parent()
            if state == 'restart':
                unreal.WidgetLibrary.set_focus_to_game_viewport()
                state = 'restart_capture'
                at = elapsed + .5
            else:
                state = 'start_attack'
            return
        controller = boss.get_controller() if state in ('start_attack','after_attack','autonomous','kill_attack') else None
        if state == 'start_attack':
            position((300,600,500)[skill_index])
            assert controller.try_execute_skill(skills[skill_index]), 'Could not start boss attack'
            state = 'windup'
            at = elapsed + .4
        elif state == 'windup':
            assert boss.get_editor_property('pattern_active') and not boss.get_editor_property('pattern_recovering')
            sample(str(skills[skill_index]) + '_windup')
            state = 'recovery'
        elif state == 'recovery':
            if not boss.get_editor_property('pattern_recovering'):
                return
            sample(str(skills[skill_index]) + '_recovery')
            state = 'after_attack'
            at = elapsed + 2
        elif state == 'after_attack':
            assert not boss.get_editor_property('pattern_active')
            skill_index += 1
            if skill_index == 2:
                position(300)
                assert controller.try_execute_skill(15106), 'Transition must interrupt an active attack'
                unreal.SystemLibrary.execute_console_command(world, 'PGBossDamage 15000')
                assert boss.get_editor_property('boss_phase') == 2 and boss.is_boss_transitioning()
                assert not boss.get_editor_property('pattern_active')
                assert not controller.try_execute_skill(15108), 'Transition allowed a new attack'
                sample('transition')
                unreal.SystemLibrary.execute_console_command(world, 'PGBossDamage 1')
                state = 'start_attack'
                at = elapsed + 1.5
            elif skill_index == 3:
                position(500)
                unreal.log('PGBossProbe autonomous BEGIN')
                controller.set_combat_thinking_enabled(True)
                state = 'autonomous'
                at = elapsed + 18
            else:
                state = 'start_attack'
        elif state == 'autonomous':
            unreal.log('PGBossProbe autonomous END')
            controller.set_combat_thinking_enabled(False)
            state = 'kill_attack'
            at = elapsed + 7.1  # Let real cooldowns expire; never reset them in the probe.
        elif state == 'kill_attack':
            position(500)
            assert controller.try_execute_skill(15107), 'Death must interrupt an active attack'
            unreal.SystemLibrary.execute_console_command(world, 'PGBossDamage 1000000')
            assert not boss.get_editor_property('pattern_active') and not boss.is_boss_transitioning()
            sample('defeated')
            state = 'death_cleanup'
            at = elapsed + 1.8
        elif state == 'death_cleanup':
            unreal.SystemLibrary.execute_console_command(world, 'PGCombatStats')
            unreal.log('PGBossProbe death_cleanup PASS')
            state = 'results'
            at = elapsed + 1.5
        elif state == 'results':
            assert any(isinstance(w, unreal.PGUIWindowRewardSelect) and w.is_in_viewport()
                       for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, True)), 'Missing delayed result window'
            sample('results')
            state = 'restart_request'
            at = elapsed + .6
        elif state == 'restart_request':
            unreal.SystemLibrary.execute_console_command(world, 'PGStartStage 6')
            state = 'restart'
            at = elapsed
        elif state == 'restart_capture':
            sample('restart')
            unreal.log('PGBossProbe COMPLETE ' + json.dumps(dict(screenshots=shots, assisted=True, direct_input=False)))
            state = 'done'
            at = elapsed + .5
        elif state == 'done':
            finish()
    except Exception as error:
        unreal.log_error('PGBossProbe FAILED state=' + state + ' ' + repr(error))
        finish()


handle = unreal.register_slate_post_tick_callback(tick)

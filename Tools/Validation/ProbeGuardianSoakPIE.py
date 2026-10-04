"""Rendered, real-time AI load. Isolated/assisted; no player-input or balance claim."""
import json
import math
import re
import time
import traceback
from pathlib import Path
import unreal

command_line = unreal.SystemLibrary.get_command_line()
assert '-PGTestProfile=GuardianSoak_' in command_line
config_match = re.search(r'-PGGuardianSoakConfig="([^"]+)"|-PGGuardianSoakConfig=([^\s]+)', command_line)
assert config_match
config = json.loads(Path(config_match[1] or config_match[2]).read_text(encoding='utf-8'))
out = Path(config['out'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
# Offscreen measurement must continue ticking/rendering while the editor has no focus.
# This only changes this process's CDO; no user preferences are saved.
unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground', False)
unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.LevelEditorPlaySettings')).set_editor_property('EnableGameSound', True)
unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.LevelEditorMiscSettings')).set_editor_property('bAllowBackgroundAudio', True)
assert level.load_level('/Game/Maps/RogueArena')
stage_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(
    unreal.load_asset('/Game/DataCenter/DataTables/Stage/DT_StageData')))
pack = next(row for row in stage_rows if row['Id'] == 3)['Waves'][2]['MonsterSpawnInfos']
assert {(row['MonsterId'], row['SpawnCount']) for row in pack} == {(15103, 2), (15102, 3)}
report = dict(status='RUNNING', assisted=True, direct_input=False, scripted_reposition=True,
              player_attacks=False, navigation_validation=False, stages=[], samples=[], config=config)
weapon_class = unreal.load_class(None, '/Script/PGActor.PGWeaponBase')
started = time.monotonic()
phase_started = None
phase_index = -1
stopped = None
actors = []
player = None
next_sample = next_state = next_reset = 0
recording = capturing = False
last_states = {}
level.editor_request_begin_play()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def console(world, value):
    unreal.SystemLibrary.execute_console_command(world, value)


def persist():
    (out / 'observations.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def stop_recording(world):
    global recording
    if recording:
        unreal.AudioMixerLibrary.stop_recording_output(world, unreal.AudioRecordingExportType.WAV_FILE,
            report['stages'][-1]['name'], str(out))
        recording = False
        console(world, 'au.NeverDisableSubmixes 0')


def destroy_pack():
    for actor in actors:
        controller = actor.get_controller()
        if controller:
            controller.set_combat_thinking_enabled(False)
        actor.destroy_actor()
        if controller:
            controller.destroy_actor()
    actors.clear()
    last_states.clear()


def arrange(world):
    origin = player.get_actor_location()
    indices = {True: 0, False: 0}
    for actor in actors:
        guardian = actor.actor_has_tag('PGGuardianRole_15103')
        index = indices[guardian]
        indices[guardian] += 1
        count = sum(a.actor_has_tag('PGGuardianRole_15103') == guardian for a in actors)
        angle = math.tau * index / min(count, 10)
        # Keep a front ring within slam range even when navigation is unavailable.
        # This is an attack/presentation load fixture, never a pathfinding verdict.
        radius = (210 if guardian else 550) + (index // 10) * 110
        point = origin + unreal.Vector(math.cos(angle)*radius, math.sin(angle)*radius, 0)
        actor.get_controller().set_combat_thinking_enabled(False)
        actor.set_actor_location_and_rotation(point, unreal.MathLibrary.find_look_at_rotation(point, origin), False, True)
        actor.get_controller().set_combat_thinking_enabled(True)


def start_phase(world, now):
    global phase_index, phase_started, next_sample, next_state, next_reset, capturing
    phase_index += 1
    phase_started = now
    spec = config['phases'][phase_index]
    destroy_pack()
    console(world, 'PGGuardianScenario '+str(spec['packs']))
    actors.extend(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGCharacterEnemy)
                  if a.actor_has_tag('PGGuardianSoak'))
    assert len(actors) == spec['packs']*5, 'Scenario spawn count mismatch'
    report['capsule_hit_queries_preserved'] = all(
        a.get_component_by_class(unreal.CapsuleComponent).get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
        for a in actors)
    report['skeletal_collision_disabled'] = all(
        a.get_component_by_class(unreal.SkeletalMeshComponent).get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        for a in actors)
    arrange(world)
    console(world, 'PGStress 0 0')
    unreal.SystemLibrary.collect_garbage()
    record = dict(name=spec['name'], enemies=len(actors), guardians=spec['packs']*2,
                  duration_requested=spec['seconds'], windup_entries=0, recovery_entries=0,
                  movement_observations=0, reposition_count=1, game_time_start=unreal.GameplayStatics.get_time_seconds(world))
    report['stages'].append(record)
    next_sample = now
    next_state = now
    next_reset = now + 60
    capturing = False
    unreal.log('PGGuardianSoak phase='+json.dumps(record))
    persist()


def finish(world, error=None):
    global stopped, player, capturing
    if stopped is not None:
        return
    stop_recording(world)
    if capturing:
        console(world, 'CsvProfile STOP')
        capturing = False
    report['status'] = 'FAIL' if error else 'PASS'
    if error:
        report['error'] = error
        unreal.log_error('PGGuardianSoak '+error)
    destroy_pack()
    report['remaining_tagged_enemies'] = len([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGCharacterEnemy)
        if a.actor_has_tag('PGGuardianSoak')]) if world else None
    report['weapons_after_cleanup'] = len(unreal.GameplayStatics.get_all_actors_of_class(world, weapon_class)) if world else None
    if world and report['weapons_after_cleanup'] != report.get('weapons_before'):
        report['status'] = 'FAIL'
        report['cleanup_error'] = 'Weapon actors were retained after fixture cleanup'
    persist()
    player = None
    level.editor_request_end_play()
    stopped = time.monotonic()


def tick(_dt):
    global player, phase_started, next_sample, next_state, next_reset, capturing, recording
    now = time.monotonic()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and now-stopped > 5:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    try:
        if now-started > sum(s['seconds'] for s in config['phases'])+180:
            raise RuntimeError('Soak watchdog expired')
        if not world:
            return
        if phase_started is None:
            player = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not player or now-started < 8:
                return
            console(world, 't.MaxFPS 60')
            console(world, 'r.VSync 0')
            console(world, 't.IdleWhenNotForeground 0')
            unreal.GameplayStatics.set_game_paused(world, False)
            console(world, 'DisableAllScreenMessages')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False):
                if widget.is_in_viewport() and not isinstance(widget, unreal.PGUIMainHUD):
                    widget.remove_from_parent()
            report['render_settings'] = {key: unreal.SystemLibrary.get_console_variable_float_value(key)
                for key in ('r.ScreenPercentage', 'sg.ViewDistanceQuality', 'sg.ShadowQuality',
                            'sg.PostProcessQuality', 'sg.TextureQuality', 'sg.EffectsQuality', 'r.VSync', 't.MaxFPS')}
            report['weapons_before'] = len(unreal.GameplayStatics.get_all_actors_of_class(world, weapon_class))
            start_phase(world, now)
            return
        spec = config['phases'][phase_index]
        elapsed = now-phase_started
        record = report['stages'][-1]
        warmup = config['warmup_seconds']
        if config['capture_images'] and not record.get('screenshot_requested') and elapsed >= 2:
            console(world, 'HighResShot 1280x720 filename="'+(out/(spec['name']+'.png')).as_posix()+'"')
            record['screenshot_requested'] = True
        if not record.get('audio_started') and elapsed >= 2:
            console(world, 'au.NeverDisableSubmixes 1')
            unreal.AudioMixerLibrary.start_recording_output(world, 15.0)
            record['audio_started'] = elapsed
            recording = True
        if not capturing and elapsed >= warmup:
            console(world, 'CsvProfile STARTFILE='+config['run_id']+'_'+spec['name']+'.csv')
            console(world, 'CsvProfile START')
            capturing = True
        if recording and elapsed >= record['audio_started']+15:
            stop_recording(world)
        if now >= next_reset:
            arrange(world)
            record['reposition_count'] += 1
            console(world, 'PGStress 0 0')
            next_reset = now+60
        if now >= next_state:
            for actor in actors:
                if actor.actor_has_tag('PGGuardianRole_15103'):
                    state = (bool(actor.get_editor_property('pattern_active')), bool(actor.get_editor_property('pattern_recovering')))
                    previous = last_states.get(actor.get_name(), (False, False))
                    record['windup_entries'] += int(state[0] and not previous[0])
                    record['recovery_entries'] += int(state[1] and not previous[1])
                    record['movement_observations'] += int(actor.get_velocity().length() > 50)
                    last_states[actor.get_name()] = state
            next_state = now+.2
        if now >= next_sample:
            enemies = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGCharacterEnemy)
            tagged = [a for a in enemies if a.actor_has_tag('PGGuardianSoak')]
            armor = sum(a.get_component_by_class(unreal.PGEnemyPresentationComponent).get_armor_count()
                        for a in tagged if a.actor_has_tag('PGGuardianRole_15103'))
            assert len(tagged) == spec['packs']*5, 'Unexpected enemy loss'
            assert armor == spec['packs']*6, 'Armor count mismatch'
            report['samples'].append(dict(phase=spec['name'], elapsed=round(elapsed, 2), enemies=len(enemies),
                tagged=len(tagged), armor=armor, actors=len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)),
                weapons=len(unreal.GameplayStatics.get_all_actors_of_class(world, weapon_class)),
                game_time=unreal.GameplayStatics.get_time_seconds(world), paused=unreal.GameplayStatics.is_game_paused(world)))
            persist()
            next_sample = now+5
        if elapsed >= spec['seconds']:
            stop_recording(world)
            console(world, 'CsvProfile STOP')
            capturing = False
            record['duration_observed'] = elapsed
            record['game_seconds_observed'] = unreal.GameplayStatics.get_time_seconds(world)-record['game_time_start']
            assert record['game_seconds_observed'] >= .9*spec['seconds'], 'Game clock did not keep up with wall time'
            assert record['windup_entries'] > 0 and record['recovery_entries'] > 0, 'AI did not attack'
            if phase_index+1 == len(config['phases']):
                finish(world)
            else:
                start_phase(world, now)
    except Exception:
        finish(world, traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

"""Exercise autonomous six-role BTs in isolated PIE. Does not save authored assets."""
import json
import math
from pathlib import Path
import re
import time
import unreal

assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(
    unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')))
role_ids = list(range(15101, 15107))
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
placed = []
for index in range(6):
    row = next(r for r in rows if r['EnemyID'] == role_ids[index])
    actor = actors.spawn_actor_from_class(unreal.load_class(None, row['ActorClass']), unreal.Vector(1400 + index * 200, 1400, 100))
    actor.set_editor_property('tags', ['PGCombatBTProbe_' + str(index)])
    placed.append(actor)
del actor
launch_requested = False
evidence = re.search(r'-PGCombatBTEvidence="?([^"\s]+)', unreal.SystemLibrary.get_command_line())
out = Path(evidence[1]) if evidence else None
started = time.monotonic()
enemies = []
player = None
stop_at = None
initialized = False
phase = 'warmup'
at = 0
starts = [0] * 6
recoveries = [0] * 6
previous = [(False, False)] * 6
pause_done = False
resume_starts = 0
relocated = False
origin = None
cleaned = False
boss_phase_done = False
boss_starts = 0


def finish():
    global stop_at
    if stop_at is None:
        level.editor_request_end_play()
        stop_at = time.monotonic()


def tick(dt):
    global initialized, player, phase, at, origin, pause_done, resume_starts, relocated, cleaned, boss_phase_done, boss_starts, launch_requested
    now = time.monotonic()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stop_at is not None:
        if not world and not cleaned:
            enemies.clear()
            player = None
            for actor in placed:
                actors.destroy_actor(actor)
            placed.clear()
            cleaned = True
            unreal.SystemLibrary.collect_garbage()
        if not world and cleaned and now - stop_at > 5:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now - started > 160:
        unreal.log_error('PGCombatBTProbe TIMEOUT ' + json.dumps(dict(phase=phase, starts=starts, recoveries=recoveries)))
        finish()
        return
    if not world:
        if not launch_requested:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if now - started > 3 and not unreal.NavigationSystemV1.is_navigation_being_built_or_locked(editor_world):
                unreal.log('PGCombatBTProbe editor_navigation_ready')
                level.editor_request_begin_play()
                launch_requested = True
        return
    try:
        if not initialized:
            player = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not player or now - started < 8:
                return
            unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world, 'PGStress 0 0')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False):
                if not isinstance(widget, unreal.PGUIMainHUD):
                    widget.remove_from_parent()
            found = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGCharacterEnemy)
            for index in range(6):
                enemy = next(e for e in found if e.actor_has_tag('PGCombatBTProbe_' + str(index)))
                enemy.get_controller().set_combat_thinking_enabled(False)
                enemies.append(enemy)
            origin = player.get_actor_location()
            # Actual accepted MoveTo requests prove navigability. Calling the static
            # K2 projection from Python hits UE 5.8's navigation CDO world ensure.
            for index, enemy in enumerate(enemies):
                angle = index * math.pi * 2 / 6
                radius = [260, 600, 330, 650, 650, 550][index]
                enemy.set_actor_location(unreal.Vector(origin.x + radius * math.cos(angle), origin.y + radius * math.sin(angle), origin.z), False, True)
                enemy.get_controller().set_combat_thinking_enabled(True)
            # Place two melee roles close together to exercise spatial separation while waiting.
            for index, offset in [(0, -55), (2, 55)]:
                enemies[index].set_actor_location(unreal.Vector(origin.x + 270, origin.y + offset, origin.z), False, True)
            initialized = True
            phase = 'autonomous'
            at = now
            return
        for index, enemy in enumerate(enemies):
            active = enemy.get_editor_property('pattern_active')
            recovery = enemy.get_editor_property('pattern_recovering')
            if active and not previous[index][0]:
                starts[index] += 1
            if recovery and not previous[index][1]:
                recoveries[index] += 1
            previous[index] = (active, recovery)
        if phase == 'autonomous' and min(starts) >= 1 and min(recoveries) >= 1:
            assert all(e.get_controller().is_using_combat_behavior_tree() for e in enemies), 'Role did not migrate to BT'
            unreal.log('PGCombatBTProbe autonomous=' + json.dumps(dict(starts=starts, recoveries=recoveries)))
            if out:
                unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x720 filename="{(out / "autonomous.png").as_posix()}"')
            phase = 'boss_wait'
        if phase == 'boss_wait' and enemies[5].get_editor_property('pattern_active'):
            unreal.SystemLibrary.execute_console_command(world, 'PGBossDamage 15000')
            assert enemies[5].get_editor_property('boss_phase') == 2 and enemies[5].is_boss_transitioning()
            assert not enemies[5].get_editor_property('pattern_active'), 'Phase transition did not cancel BT ability'
            boss_starts = starts[5]
            phase = 'boss_resume'
        elif phase == 'boss_resume':
            if enemies[5].is_boss_transitioning():
                assert not enemies[5].get_editor_property('pattern_active'), 'Attack started during boss transition'
            elif starts[5] > boss_starts:
                boss_phase_done = True
                if out:
                    unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x720 filename="{(out / "boss_phase_two.png").as_posix()}"')
                phase = 'cancel'
        if phase == 'cancel' and enemies[0].get_editor_property('pattern_active'):
            ai = enemies[0].get_controller()
            ai.set_combat_thinking_enabled(False)
            assert not enemies[0].get_editor_property('pattern_active'), 'BT abort left active pattern'
            resume_starts = starts[0]
            at = now
            phase = 'paused'
        elif phase == 'paused' and now - at > 1:
            assert not enemies[0].get_editor_property('pattern_active'), 'Paused BT restarted an attack'
            enemies[0].get_controller().set_combat_thinking_enabled(True)
            pause_done = True
            phase = 'resumed'
        elif phase == 'resumed' and starts[0] > resume_starts:
            # Stop/restart during an attack, then move the player. The agents must keep making progress.
            for enemy in enemies:
                enemy.get_controller().set_combat_thinking_enabled(False)
            player.set_actor_location(unreal.Vector(origin.x + 500, origin.y, origin.z), False, True)
            for enemy in enemies:
                enemy.get_controller().set_combat_thinking_enabled(True)
            resume_starts = sum(starts)
            phase = 'relocated'
        elif phase == 'relocated' and sum(starts) > resume_starts + 2:
            relocated = True
            for enemy in enemies:
                enemy.get_controller().set_combat_thinking_enabled(False)
                assert not enemy.get_editor_property('pattern_active')
            unreal.log('PGCombatBTProbe COMPLETE ' + json.dumps(dict(starts=starts, recoveries=recoveries,
                roles=role_ids, pause_resume=pause_done, target_relocation=relocated, boss_phase=boss_phase_done,
                position_moves=[e.get_controller().get_position_move_count() for e in enemies], assisted=True, direct_input=False)))
            finish()
    except Exception as error:
        unreal.log_error('PGCombatBTProbe FAILED ' + repr(error))
        finish()


handle = unreal.register_slate_post_tick_callback(tick)

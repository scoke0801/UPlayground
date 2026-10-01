"""Isolated rendered role-pattern probe. No authored assets are saved; no player input is injected."""
import json
import time
import unreal

assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
table = unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
placed = []
for row in rows:
    if 15101 <= row['EnemyID'] <= 15105:
        cls = unreal.load_class(None, row['ActorClass'])
        actor = editor_actors.spawn_actor_from_class(cls, unreal.Vector(1400, 1400, 100))
        actor.set_editor_property('tags', ['PGRoleProbe_' + str(row['EnemyID'])])
        placed.append(actor)
level.editor_request_begin_play()
started = time.monotonic()
initialized = False
stop_requested = None
enemies = {}
player = None
phase = 0
index = 0
at = 0
elapsed = 0
origin = None
shots = 0
observed = []
cleaned = False


def hide_enemy(enemy, hidden):
    enemy.set_actor_hidden_in_game(hidden)
    enemy.set_actor_enable_collision(not hidden)
    for child in enemy.get_attached_actors():
        child.set_actor_hidden_in_game(hidden)


def log_sample(enemy_id, state, enemy):
    global shots
    unreal.log('PGContentProbe sample=' + json.dumps(dict(enemy=enemy_id, state=state,
        active=enemy.get_editor_property('pattern_active'), recovery=enemy.get_editor_property('pattern_recovering'),
        guard=enemy.get_editor_property('guarding'), position=str(enemy.get_actor_location()))))
    unreal.SystemLibrary.execute_console_command(enemy, 'PGCombatStats')
    unreal.SystemLibrary.execute_console_command(enemy, 'Shot SHOWUI')
    shots += 1


def finish(world):
    global stop_requested
    if stop_requested is None:
        level.editor_request_end_play()
        stop_requested = time.monotonic()


def tick(dt):
    global initialized, player, origin, phase, index, at, elapsed, stop_requested, cleaned
    now = time.monotonic()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stop_requested is not None:
        if not world and not cleaned:
            enemies.clear()
            player = None
            for actor in placed:
                editor_actors.destroy_actor(actor)
            placed.clear()
            cleaned = True
            unreal.SystemLibrary.collect_garbage()
        if not world and cleaned and now - stop_requested > 4:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now - started > 130:
        unreal.log_error('PGContentProbe TIMEOUT')
        finish(world)
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
            unreal.SystemLibrary.execute_console_command(world, 'PGStress 0 0')  # Persists Assisted before health adjustment.
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False):
                if not isinstance(widget, unreal.PGUIMainHUD):
                    widget.remove_from_parent()
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGCharacterEnemy):
                for eid in range(15101,15106):
                    if enemy.actor_has_tag('PGRoleProbe_' + str(eid)):
                        enemies[eid] = enemy
                        enemy.get_controller().set_combat_thinking_enabled(False)
                        hide_enemy(enemy, True)
            assert len(enemies) == 5, 'Missing role actors'
            origin = player.get_actor_location()
            initialized = True
            at = elapsed + 1
            return
        if elapsed < at:
            return
        eid = 15101 + index
        enemy = enemies[eid]
        controller = enemy.get_controller()
        if phase == 0:
            distance = {15101:160,15102:600,15103:250,15104:600,15105:500}[eid]
            player.set_actor_location(origin, False, True)
            enemy.set_actor_location_and_rotation(unreal.Vector(origin.x-distance, origin.y, origin.z), unreal.Rotator(), False, True)
            hide_enemy(enemy, False)
            assert controller.try_execute_skill(eid), 'Pattern activation failed: ' + str(eid)
            phase = 1
            at = elapsed + (.2 if eid == 15101 else .4)
        elif phase == 1:
            assert enemy.get_editor_property('pattern_active')
            log_sample(eid, 'windup', enemy)
            phase = 2
            at = elapsed + .1
        elif phase == 2:
            if not enemy.get_editor_property('pattern_recovering'):
                at = elapsed + .05
                return
            log_sample(eid, 'recovery', enemy)
            observed.append(eid)
            phase = 3
            at = elapsed + 1.9
        elif phase == 3:
            controller.set_combat_thinking_enabled(False)
            assert not enemy.get_editor_property('pattern_active'), 'Cancel left active state'
            hide_enemy(enemy, True)
            index += 1
            if index == 5:
                unreal.log('PGContentProbe COMPLETE ' + json.dumps(dict(roles=observed, screenshots=shots, assisted=True, direct_input=False)))
                finish(world)
            else:
                phase = 0
                at = elapsed + 1
    except Exception as error:
        unreal.log_error('PGContentProbe FAILED ' + repr(error))
        finish(world)


handle = unreal.register_slate_post_tick_callback(tick)

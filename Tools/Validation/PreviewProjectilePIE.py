"""Observe skill 15102 firing the authored mesh in RogueArena; save no map changes.

Run in UnrealEditor with -ExecutePythonScript=<this file>, -RenderOffscreen,
-culture=en and an isolated -PGTestProfile=ProjectileArt_<unique id>.
"""
import json
import time
import traceback
from pathlib import Path

import unreal

assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
OUT = Path(unreal.Paths.project_saved_dir())/'ProjectileArt'
OUT.mkdir(parents=True, exist_ok=True)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
table = unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')
row = next(row for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
           if row['EnemyID'] == 15102)
placed = actors.spawn_actor_from_class(unreal.load_class(None, row['ActorClass']), unreal.Vector(1400, 1400, 100))
placed.set_editor_property('tags', ['PGProjectileArtProbe'])
level.editor_request_begin_play()
started = time.monotonic()
phase, at, stopped = 0, 0.0, None
enemy = player = bolt = first_location = None
report = {'status': 'RUNNING', 'assisted': True, 'direct_input': False}
(OUT/'presentation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def finish(world, error=None):
    global stopped, bolt, enemy, player
    if stopped is not None:
        return
    if world:
        unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
    report['status'] = 'FAIL' if error else 'PASS'
    if error:
        report['error'] = error
        unreal.log_error('PGProjectilePreview FAIL ' + error)
    else:
        unreal.log('PGProjectilePreview PASS ' + json.dumps(report))
    (OUT/'presentation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    bolt = enemy = player = None
    level.editor_request_end_play()
    stopped = time.monotonic()


def tick(delta):
    global phase, at, enemy, player, bolt, first_location, placed
    now = time.monotonic()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and placed:
            actors.destroy_actor(placed)
            placed = None
            unreal.SystemLibrary.collect_garbage()
        if not world and now-stopped > 3:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now-started > 100:
        finish(world, 'Timed out waiting for projectile observation')
        return
    if not world or now < at:
        return
    try:
        if phase == 0:
            player = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not player or now-started < 6:
                return
            unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world, 'PGStress 0 0')
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False):
                if widget.is_in_viewport() and not isinstance(widget, unreal.PGUIMainHUD):
                    widget.remove_from_parent()
            enemy = next(actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGCharacterEnemy)
                         if actor.actor_has_tag('PGProjectileArtProbe'))
            enemy.get_controller().set_combat_thinking_enabled(False)
            origin = player.get_actor_location()
            enemy.set_actor_location_and_rotation(unreal.Vector(origin.x-650, origin.y, origin.z), unreal.Rotator(), False, True)
            # Autonomous AI may have fired during editor startup. Let its
            # existing cooldown expire after disabling decisions.
            phase, at = -1, now+3.5
        elif phase == -1:
            assert enemy.get_controller().try_execute_skill(15102), 'Shooter skill did not activate after cooldown'
            phase = 1
        elif phase == 1:
            projectiles = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGPatternProjectile)
            if not projectiles:
                return
            assert len(projectiles) == 1
            bolt = projectiles[0]
            component = bolt.get_editor_property('mesh_component')
            assert component.get_editor_property('static_mesh').get_path_name() == '/Game/Art/Projectiles/SM_PG_CrystalBolt.SM_PG_CrystalBolt'
            assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
            box = bolt.get_editor_property('projectile_collision_box').get_unscaled_box_extent()
            assert abs(box.y-22.0) < .01
            first_location = bolt.get_actor_location()
            report['mesh'] = component.get_editor_property('static_mesh').get_path_name()
            report['collision_extent'] = [box.x, box.y, box.z]
            phase = 2
        elif phase == 2:
            location = bolt.get_actor_location()
            travelled = (location-first_location).length()
            if travelled < 170:
                return
            velocity = bolt.get_editor_property('movement_component').get_editor_property('velocity')
            assert abs(velocity.length()-950) < 1, str(velocity)
            assert location.x > first_location.x and abs(location.y-first_location.y) < 1
            assert bolt.get_actor_forward_vector().x > .99
            report.update(speed=velocity.length(), observed_travel_cm=travelled, forward='+X')
            unreal.GameplayStatics.set_global_time_dilation(world, .01)
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI')
            phase, at = 3, now+2
        elif phase == 3:
            unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
            phase, at = 4, now+2
        elif phase == 4:
            assert not unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PGPatternProjectile), 'Projectile did not clear after contact'
            report['cleared_after_contact'] = True
            finish(world)
    except Exception as error:
        finish(world, traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

"""Render each new attack, then observe a mixed pack. Uses assisted health, no player input."""
import json
from pathlib import Path
import re
import time
import unreal

assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
evidence = re.search(r'-PGVarietyEvidence="?([^"\s]+)',unreal.SystemLibrary.get_command_line())
assert evidence, 'Missing isolated screenshot destination'
OUT = Path(evidence[1])
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
table = unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
placed = []
for row in rows:
    if 15101 <= row['EnemyID'] <= 15106:
        actor = editor_actors.spawn_actor_from_class(unreal.load_class(None,row['ActorClass']), unreal.Vector(1400,1400,100))
        actor.set_editor_property('tags', ['PGVariety_' + str(row['EnemyID'])])
        placed.append(actor)
level.editor_request_begin_play()
started = time.monotonic()
stop_requested = None
enemies = {}
player = origin = None
elapsed = at = 0.
index = shots = 0
phase = 'setup'
cleaned = False
samples = []
max_active = mixed_ticks = 0
mixed_seen = set()
cases = [(15101,15111,260),(15102,15112,650),(15103,15113,360),
         (15104,15114,300),(15105,15115,330),(15104,15116,200),(15106,15109,450)]


def hide(enemy, value):
    enemy.set_actor_hidden_in_game(value)
    enemy.set_actor_enable_collision(not value)
    for child in enemy.get_attached_actors(): child.set_actor_hidden_in_game(value)


def finish():
    global stop_requested
    if stop_requested is None:
        level.editor_request_end_play()
        stop_requested = time.monotonic()


def sample(eid, sid, state):
    global shots
    enemy = enemies[eid]
    row = dict(enemy=eid,skill=sid,state=state,active=enemy.get_editor_property('pattern_active'),
               recovering=enemy.get_editor_property('pattern_recovering'))
    samples.append(row)
    unreal.log('PGVarietyProbe sample=' + json.dumps(row))
    unreal.SystemLibrary.execute_console_command(player, 'HighResShot 1280x720 filename="' + (OUT/(str(sid)+'_'+state+'.png')).as_posix() + '"')
    shots += 1


def tick(dt):
    global player, origin, elapsed, at, phase, index, cleaned, max_active, mixed_ticks
    now = time.monotonic()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stop_requested is not None:
        if not world and not cleaned:
            enemies.clear(); player = None
            for actor in placed: editor_actors.destroy_actor(actor)
            placed.clear(); cleaned = True
            unreal.SystemLibrary.collect_garbage()
        if not world and cleaned and now-stop_requested > 4:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now-started > 160:
        unreal.log_error('PGVarietyProbe TIMEOUT phase=' + phase); finish(); return
    if not world: return
    elapsed += dt
    try:
        if phase == 'setup':
            player = unreal.GameplayStatics.get_player_pawn(world,0)
            if not player or elapsed < 2: return
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world,'PGStress 0 0')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if not isinstance(widget,unreal.PGUIMainHUD): widget.remove_from_parent()
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                for eid in range(15101,15107):
                    if enemy.actor_has_tag('PGVariety_' + str(eid)):
                        enemies[eid] = enemy
                        enemy.get_controller().set_combat_thinking_enabled(False)
                        hide(enemy,True)
            assert len(enemies) == 6
            # Exercise the actual GAS phase transition before the new phase-two attack.
            unreal.SystemLibrary.execute_console_command(world,'PGBossDamage 15000')
            assert enemies[15106].get_editor_property('boss_phase') == 2
            origin = player.get_actor_location()
            unreal.WidgetLibrary.set_focus_to_game_viewport()
            phase = 'start'; at = elapsed + 2
            return
        if phase == 'mixed':
            active = [eid for eid in range(15101,15106) if enemies[eid].get_editor_property('pattern_active')]
            max_active = max(max_active,len(active)); mixed_ticks += 1; mixed_seen.update(active)
            assert len(active) <= 3, 'Concurrent attack budget exceeded'
            if elapsed < at: return
            assert mixed_ticks > 300 and len(mixed_seen) == 5, ('Starved roles',mixed_seen,mixed_ticks)
            unreal.log('PGVarietyProbe COMPLETE ' + json.dumps(dict(screenshots=shots,skills=7,mixed_ticks=mixed_ticks,
                max_concurrent=max_active,participating_roles=sorted(mixed_seen),assisted=True,direct_input=False)))
            phase = 'done'; at = elapsed + .5
            return
        if elapsed < at: return
        if phase == 'done': finish(); return
        eid,sid,distance = cases[index]
        enemy = enemies[eid]
        controller = enemy.get_controller()
        if phase == 'start':
            player.set_actor_location(origin,False,True)
            enemy.set_actor_location_and_rotation(unreal.Vector(origin.x-distance,origin.y,origin.z),unreal.Rotator(),False,True)
            hide(enemy,False)
            assert controller.try_execute_skill(sid), 'Could not start skill ' + str(sid)
            phase = 'windup'; at = elapsed + .3
        elif phase == 'windup':
            assert enemy.get_editor_property('pattern_active') and not enemy.get_editor_property('pattern_recovering')
            sample(eid,sid,'windup'); phase = 'recovery'
        elif phase == 'recovery':
            if not enemy.get_editor_property('pattern_recovering'): return
            sample(eid,sid,'recovery'); phase = 'cleanup'; at = elapsed + .35
        elif phase == 'cleanup':
            controller.set_combat_thinking_enabled(False)
            assert not enemy.get_editor_property('pattern_active')
            hide(enemy,True)
            index += 1
            if index == len(cases):
                positions = [(-220,0),(-620,200),(-320,-200),(450,0),(450,350)]
                for role,xy in zip(range(15101,15106),positions):
                    unit = enemies[role]
                    unit.set_actor_location(unreal.Vector(origin.x+xy[0],origin.y+xy[1],origin.z),False,True)
                    hide(unit,False); unit.get_controller().set_combat_thinking_enabled(True)
                phase = 'mixed'; at = elapsed + 25
            else:
                phase = 'start'; at = elapsed + .5
    except Exception as error:
        unreal.log_error('PGVarietyProbe FAILED phase=' + phase + ' ' + repr(error)); finish()


handle = unreal.register_slate_post_tick_callback(tick)

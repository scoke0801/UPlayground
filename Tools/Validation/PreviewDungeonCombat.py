"""Capture P1 traversal guidance, room encounter and reward selection in Korean."""
import json
import time
import traceback
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve()/'Saved/QA/ProceduralDungeon/P1/Presentation'
OUT.mkdir(parents=True, exist_ok=True)
L = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert L.load_level('/Game/Maps/L_PG_ProceduralDungeon')
u.EditorPythonScripting.set_keep_python_script_alive(True)
assert u.PGEditorProbeTools.begin_play_window(1600, 900)
started = time.monotonic()
stopped = None
at = started
phase = 'ready'
report = dict(status='RUNNING', images=[], direct_play=False)

def capture(world, name):
    path = OUT/(name+'.png')
    assert u.PGEditorProbeTools.capture_game_viewport(world, str(path))
    report['images'].append(str(path))

def finish(error=None):
    global stopped
    report['status'] = 'FAIL' if error else 'PASS'
    if error:
        report['error'] = error
        u.log_error(error)
    else:
        u.log('PGDungeonCombat RENDER PASS')
    (OUT/'render.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    L.editor_request_end_play()
    stopped = time.monotonic()

def tick(dt):
    global phase, at
    now = time.monotonic()
    if stopped:
        if now-stopped > 4 and not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
            u.unregister_slate_post_tick_callback(handle); u.SystemLibrary.quit_editor()
        return
    try:
        assert now-started < 240, phase
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world: return
        g = u.GameplayStatics.get_actor_of_class(world,u.PGDungeonGenerator)
        if not g: return
        assert g.get_editor_property('state') != u.PGDungeonState.FAILED
        if g.get_editor_property('state') != u.PGDungeonState.READY: return
        m = g.get_editor_property('combat_manager')
        player = u.GameplayStatics.get_player_pawn(world,0)
        if not m or not player: return
        for enemy in u.GameplayStatics.get_all_actors_of_class(world,u.PGCharacterEnemy):
            ai=enemy.get_controller()
            if ai and isinstance(ai,u.PGRoleAIController): ai.set_combat_thinking_enabled(False)
        if phase == 'ready':
            u.SystemLibrary.execute_console_command(world,'t.MaxFPS 30')
            phase='entrance'; at=now
        elif phase == 'entrance' and now-at > 5:
            capture(world,'01_traversal')
            player.set_actor_location(m.get_dungeon_objective_location()+u.Vector(-650,-650,100),False,True)
            phase='combat'; at=now
        elif phase == 'combat' and m.get_spawned_monsters() >= 3 and now-at > 4:
            capture(world,'02_room_combat')
            u.GameplayStatics.set_global_time_dilation(world,10.)
            phase='clear'
        elif phase == 'clear':
            u.SystemLibrary.execute_console_command(world,'PGDungeonStep kill')
            if m.get_current_stage_state() == u.PGStageState.BUILD_PHASE:
                u.GameplayStatics.set_global_time_dilation(world,1.)
                phase='reward'; at=now
        elif phase == 'reward' and now-at > 2:
            capture(world,'03_reward')
            finish()
    except BaseException:
        finish(traceback.format_exc())

handle = u.register_slate_post_tick_callback(tick)

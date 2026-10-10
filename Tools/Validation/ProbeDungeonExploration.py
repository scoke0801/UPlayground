"""Exercise discovery, revisit, regeneration and cancellation in a real PIE world."""
import json
import time
import traceback
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve()/'Saved/QA/ProceduralDungeon/P2'
OUT.mkdir(parents=True, exist_ok=True)
RENDER = 'PGDungeonMapRender' in u.SystemLibrary.get_command_line()
L = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert L.load_level('/Game/Maps/L_PG_ProceduralDungeon')
u.EditorPythonScripting.set_keep_python_script_alive(True)
assert u.PGEditorProbeTools.begin_play_window(1600, 900)
started = time.monotonic()
stopped = None
phase = 'ready'
at = started
visited = 1
report = dict(status='RUNNING', images=[], direct_play=False)

def finish(error=None):
    global stopped
    report['status'] = 'FAIL' if error else 'PASS'
    if error:
        report['error'] = error
        u.log_error(error)
    else:
        u.log('PGDungeonExploration PROBE PASS')
    (OUT/('render.probe.json' if RENDER else 'runtime.probe.json')).write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    L.editor_request_end_play()
    stopped = time.monotonic()

def capture(world, name):
    if not RENDER: return
    path = OUT/(name+'.png')
    assert u.PGEditorProbeTools.capture_game_viewport(world, str(path))
    report['images'].append(str(path))

def tick(dt):
    global phase, at, visited
    now = time.monotonic()
    if stopped:
        if now-stopped > 4 and not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()
        return
    try:
        assert now-started < 240, phase
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world: return
        g = u.GameplayStatics.get_actor_of_class(world, u.PGDungeonGenerator)
        player = u.GameplayStatics.get_player_pawn(world, 0)
        if not g or not player: return
        discovery = g.get_discovery()
        state = g.get_editor_property('state')
        assert state != u.PGDungeonState.FAILED
        if phase == 'cancelled':
            assert state == u.PGDungeonState.CANCELLED
            assert discovery.get_discovered_room_count() == 0
            g.generate(-2147483648)
            phase='recovered'; at=now
            return
        if state != u.PGDungeonState.READY: return
        rooms = g.get_editor_property('layout').rooms
        definition = g.get_editor_property('definition')
        spacing = definition.get_editor_property('room_size') + definition.get_editor_property('corridor_length')
        origin = g.get_actor_location()
        def move(room):
            cell = room.cell
            player.set_actor_location(origin+u.Vector(cell.x*spacing, cell.y*spacing,100), False, True)
        delay = 3 if RENDER else .25
        if phase == 'ready':
            phase='entrance'; at=now
        elif phase == 'entrance' and now-at > delay:
            assert discovery.get_discovered_room_count() == 1
            assert discovery.get_known_connection_count() == 0
            capture(world, '01_entrance')
            move(rooms[visited]); phase='visiting'; at=now
        elif phase == 'visiting' and now-at > delay:
            assert discovery.get_discovered_room_count() == visited+1
            visited += 1
            if visited < len(rooms):
                move(rooms[visited]); at=now
            else:
                assert discovery.get_known_connection_count() == len(g.get_editor_property('layout').connections)
                report['rooms'] = len(rooms)
                capture(world, '02_discovered')
                move(rooms[0]); phase='revisit'; at=now
        elif phase == 'revisit' and now-at > delay:
            assert discovery.get_discovered_room_count() == visited
            capture(world, '03_revisited')
            g.generate(17); phase='regenerated'; at=now
        elif phase in ('regenerated', 'recovered') and now-at > delay:
            assert discovery.get_discovered_room_count() == 1
            assert discovery.get_known_connection_count() == 0
            if phase == 'recovered':
                report['regeneration_and_cancel_recovery'] = True
                finish()
            else:
                g.cancel_generation(); phase='cancelled'
    except BaseException:
        finish(traceback.format_exc())

handle = u.register_slate_post_tick_callback(tick)

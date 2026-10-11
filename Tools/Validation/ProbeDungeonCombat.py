"""Assisted PIE acceptance of P1. Does not replace direct combat/visual acceptance."""
import json
import os
import time
import traceback
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = Path(os.environ.get('PG_DUNGEON_QA_DIR',str(ROOT / 'Saved/QA/ProceduralDungeon/P1')))
OUT.mkdir(parents=True, exist_ok=True)
assert '-PGTestProfile=' in u.SystemLibrary.get_command_line()
L = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert L.load_level('/Game/Maps/L_PG_ProceduralDungeon')
u.EditorPythonScripting.set_keep_python_script_alive(True)
assert u.PGEditorProbeTools.begin_play_window(1280, 720)
started = time.monotonic()
stopped = None
phase = 'ready'
at = started
seeds = [101026, 17, -2147483648]
run = 0
seen = set()
budgets = {}
spawned = {}
waves = set()
save_tested = False
report = dict(status='RUNNING', assisted=True, direct_play=False, runs=[], recovery=[])
report['barrier_checks']=[]
barrier_checks=set()

def check_barriers(generator, manager, player, state):
    stage=manager.get_current_stage_id()
    sealed=state in (u.PGStageState.IN_PROGRESS,u.PGStageState.WAVE_INTERMISSION)
    encounter=[c for c in generator.get_components_by_class(u.StaticMeshComponent)
               if 'PGDungeonEncounterGate' in [str(t) for t in c.component_tags]]
    center=manager.get_dungeon_objective_location()
    active=[]
    for c in encounter:
        delta=c.get_world_location()-center
        belongs=abs(delta.x)<1500 and abs(delta.y)<1500
        expected=sealed and belongs
        assert (c.get_collision_enabled()!=u.CollisionEnabled.NO_COLLISION)==expected,('barrier state',stage,state,c.get_name())
        if belongs: active.append(c)
    key=(run,stage,str(state))
    if key in barrier_checks or stage==6: return
    assert active,('No socket barriers',stage)
    original=player.get_actor_location()
    try:
        for c in active:
            p=c.get_world_location(); delta=p-center
            direction=u.Vector((1 if delta.x>0 else -1) if abs(delta.x)>abs(delta.y) else 0,
                               (1 if delta.y>0 else -1) if abs(delta.y)>abs(delta.x) else 0,0)
            p.z=center.z+100
            start=p-direction*200; end=p+direction*200
            player.set_actor_location(start,False,True)
            player.set_actor_location(end,True,False)
            remaining=(player.get_actor_location()-end).length()
            assert (remaining>100) if sealed else (remaining<2),('capsule crossing',stage,state,remaining)
    finally:
        player.set_actor_location(original,False,True)
    barrier_checks.add(key)
    report['barrier_checks'].append(dict(seed=generator.get_editor_property('seed'),stage=stage,state=str(state),sealed=sealed,sockets=len(active)))

def command(world, text):
    u.SystemLibrary.execute_console_command(world, text)

def actors(world):
    return u.GameplayStatics.get_all_actors_of_class(world, u.PGCharacterEnemy)

def gates(generator):
    return [c for c in generator.get_components_by_class(u.StaticMeshComponent)
            if not isinstance(c, u.InstancedStaticMeshComponent) and 'PGDungeonGate' in [str(t) for t in c.component_tags]]

def enter(player, manager):
    player.set_actor_location(manager.get_dungeon_objective_location() + u.Vector(0, 0, 100), False, True)

def finish(error=None):
    global stopped
    report['status'] = 'FAIL' if error else 'PASS'
    if error:
        report['error'] = error
        u.log_error(error)
    else:
        u.log('PGDungeonCombat PROBE PASS')
    (OUT / 'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    L.editor_request_end_play()
    stopped = time.monotonic()

def tick(dt):
    global phase, at, run, seen, budgets, spawned, waves, save_tested
    now = time.monotonic()
    if stopped is not None:
        if now - stopped > 3 and not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()
        return
    try:
        assert now - started < 1000, ('timeout', phase)
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        g = u.GameplayStatics.get_actor_of_class(world, u.PGDungeonGenerator)
        player = u.GameplayStatics.get_player_pawn(world, 0)
        if not g or not player:
            return
        if phase == 'save_generation_wait':
            if g.get_editor_property('state') != u.PGDungeonState.FAILED:
                return
            assert not g.get_editor_property('combat_manager')
            command(world, 'PGSaveFailure false')
            report['recovery'].append('new-run save failure blocks readiness')
            g.generate(23)
            phase = 'cancel_ready'
            return
        if g.get_editor_property('state') != u.PGDungeonState.READY:
            assert g.get_editor_property('state') != u.PGDungeonState.FAILED, g.get_editor_property('last_error')
            return
        m = g.get_editor_property('combat_manager')
        assert m and m.is_dungeon_combat()
        state = m.get_current_stage_state()
        stage = m.get_current_stage_id()
        for e in actors(world):
            ai = e.get_controller()
            if ai and isinstance(ai, u.PGRoleAIController):
                ai.set_combat_thinking_enabled(False)
        if phase == 'ready':
            u.GameplayStatics.set_global_time_dilation(world, 3.)
            command(world, 'PGDungeonStep status')
            assert state == u.PGStageState.DUNGEON_TRAVERSAL
            assert not actors(world)
            m.start_stage(6)
            m.ready_for_next_stage()
            assert m.get_current_stage_id() == 1
            assert m.get_current_stage_state() == u.PGStageState.DUNGEON_TRAVERSAL
            phase = 'entrance'; at = now
            return
        if phase == 'entrance':
            assert not actors(world), 'Entrance must remain safe'
            assert all(c.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION for c in gates(g))
            if now - at < 1:
                return
            phase = 'run'
        if phase == 'run':
            assert state != u.PGStageState.FAILED, 'Encounter failed'
            if state == u.PGStageState.DUNGEON_TRAVERSAL:
                enter(player, m)
                return
            if state in (u.PGStageState.IN_PROGRESS, u.PGStageState.WAVE_INTERMISSION):
                check_barriers(g,m,player,state)
                row = m.get_current_stage_data_copy()
                budgets[stage] = sum(s.spawn_count for w in row.waves for s in w.monster_spawn_infos)
                spawned[stage] = m.get_spawned_monsters()
                waves.add((stage, m.get_current_wave_number()))
                center = m.get_dungeon_objective_location()
                for e in actors(world):
                    name = e.get_name()
                    if name in seen:
                        continue
                    seen.add(name)
                    p = e.get_actor_location() - center
                    assert abs(p.x) <= 1000 and abs(p.y) <= 1000, ('spawn outside room', stage, p)
                if stage == 4:
                    command(world, 'PGDungeonStep summons')
                command(world, 'PGDungeonStep kill')
            elif state == u.PGStageState.BUILD_PHASE:
                check_barriers(g,m,player,state)
                spawned[stage] = m.get_spawned_monsters()
                if not save_tested:
                    command(world, 'PGSaveFailure true')
                    command(world, 'PGDungeonStep reward')
                    m.ready_for_next_stage()
                    assert m.get_current_stage_state() == u.PGStageState.BUILD_PHASE
                    command(world, 'PGSaveFailure false')
                    save_tested = True
                command(world, 'PGDungeonStep reward')
                # Completing all choices must arm the next room without a ready input.
                if m.get_current_stage_state() == u.PGStageState.DUNGEON_TRAVERSAL:
                    assert m.get_current_stage_id() == stage + 1
            elif state == u.PGStageState.FINISHED:
                spawned[6] = m.get_spawned_monsters()
                assert budgets == spawned, (budgets, spawned)
                assert len(waves) == 16, sorted(waves)
                command(world, 'PGDungeonStep status')
                report['runs'].append(dict(seed=seeds[run], budgets=budgets, spawned=spawned, waves=len(waves)))
                run += 1
                if run < len(seeds):
                    seen = set(); budgets = {}; spawned = {}; waves = set()
                    g.generate(seeds[run]); phase = 'ready'
                else:
                    g.generate(21); phase = 'destroy_ready'
            return
        if phase == 'destroy_ready':
            enter(player, m); phase = 'destroy'; return
        if phase == 'destroy':
            enemies = actors(world)
            if not enemies:
                return
            enemies[0].destroy_actor()
            assert m.get_current_stage_state() == u.PGStageState.FAILED
            command(world, 'PGDungeonStep status')
            phase = 'destroy_check'; at = now; return
        if phase == 'destroy_check':
            if now - at < .5: return
            assert not actors(world)
            assert all(c.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION for c in gates(g))
            assert all(c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION for c in g.get_components_by_class(u.StaticMeshComponent)
                       if 'PGDungeonEncounterGate' in [str(t) for t in c.component_tags])
            report['recovery'].append('unexpected destroy cancels encounter and unlocks boss gate')
            g.generate(22); phase = 'unreachable_ready'; return
        if phase == 'unreachable_ready':
            enter(player, m); phase = 'unreachable'; return
        if phase == 'unreachable':
            enemies = actors(world)
            if not enemies: return
            e = enemies[0]
            e.character_movement.set_movement_mode(u.MovementMode.MOVE_NONE)
            e.set_actor_location(m.get_dungeon_objective_location()+u.Vector(0,0,-500),False,True)
            phase = 'unreachable_wait'; at = now; return
        if phase == 'unreachable_wait':
            if state != u.PGStageState.FAILED:
                assert now-at < 30, 'Unreachable enemy did not cancel encounter'
                return
            command(world, 'PGDungeonStep status')
            assert not actors(world)
            report['recovery'].append('unreachable enemy cancels encounter after bounded timeout')
            g.generate(22); phase = 'spawnfail_ready'; return
        if phase == 'spawnfail_ready':
            command(world, 'PGDungeonStep spawnfail')
            enter(player, m); phase = 'spawnfail'; at = now; return
        if phase == 'spawnfail':
            if state != u.PGStageState.FAILED:
                assert now-at < 30, 'Spawn retries did not terminate'
                return
            assert m.get_spawned_monsters() == 0
            assert m.get_remaining_monsters() > 0
            command(world, 'PGDungeonStep status')
            report['recovery'].append('exhausted spawn retries cancel without treating unspawned enemies as kills')
            g.generate(22); phase = 'regen_ready'; return
        if phase == 'regen_ready':
            enter(player, m); phase = 'regen_combat'; return
        if phase == 'regen_combat':
            if not actors(world): return
            g.generate(23)
            assert not actors(world), 'Regeneration leaked enemies'
            phase = 'death_ready'; return
        if phase == 'death_ready':
            assert m.get_current_stage_state() == u.PGStageState.DUNGEON_TRAVERSAL
            assert not actors(world)
            report['recovery'].append('active combat regeneration removes enemies and stale callbacks')
            command(world, 'PGDungeonStep die')
            assert m.get_current_stage_state() == u.PGStageState.FAILED
            command(world, 'PGDungeonStep restart')
            phase = 'restart'; at = now; return
        if phase == 'restart':
            if now-at < 2: return
            assert m.get_current_stage_state() == u.PGStageState.DUNGEON_TRAVERSAL
            assert not actors(world)
            command(world, 'PGDungeonStep status')
            report['recovery'].append('death during traversal and map restart restore fresh playable run')
            command(world, 'PGSaveFailure true')
            g.generate(24)
            phase = 'save_generation_wait'; return
        if phase == 'cancel_ready':
            g.cancel_generation()
            assert g.get_editor_property('state') == u.PGDungeonState.CANCELLED
            assert not actors(world)
            assert not g.get_components_by_class(u.StaticMeshComponent)
            report['recovery'].append('cancel removes geometry, gates and enemies')
            finish()
    except BaseException:
        finish(traceback.format_exc())

handle = u.register_slate_post_tick_callback(tick)

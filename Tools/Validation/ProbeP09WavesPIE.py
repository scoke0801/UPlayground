"""Read saved waves, then exercise the actual stage manager's first-wave spawner."""
from collections import Counter
import json
from pathlib import Path
import re
import sys
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureP09Waves import validate, rows, TABLE
from P09WaveRoster import P09_IDS
validate()
command=unreal.SystemLibrary.get_command_line()
assert '-PGTestProfile=' in command
OUT=Path(re.search(r'-PGP09Evidence="?([^"\s]+)',command)[1])
OUT.mkdir(parents=True,exist_ok=True)
stages={r['Id']:r for r in rows(TABLE)}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
level.editor_request_begin_play()
started=time.monotonic()
stopped=None
manager=None
phase='setup'
sid=1
seen=set()
observed=Counter()
p09_seen=set()
at=0.
report=dict(status='RUNNING',assisted=True,direct_input=False,waves=[])


def finish(error=None):
    global stopped,manager
    if stopped is not None:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error('PGP09WaveProbe FAIL '+error)
    else:unreal.log('PGP09WaveProbe PASS '+json.dumps(report))
    (OUT/'runtime.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    manager=None
    level.editor_request_end_play()
    stopped=time.monotonic()


def tick(dt):
    global manager,phase,sid,seen,observed,at
    now=time.monotonic()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and now-stopped>3:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now-started>160:finish('Timeout '+phase+' stage='+str(sid));return
    if not world:return
    try:
        if phase=='setup':
            if now-started<6:return
            managers=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGStageManager)
            if not managers:return
            manager=managers[0]
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            phase='start'
        if phase=='start':
            seen=set();observed=Counter()
            manager.start_stage(sid)
            at=now;phase='spawn'
            return
        assert manager.get_current_stage_state()!=unreal.PGStageState.FAILED, 'Stage failed'
        expected=Counter({s['MonsterId']:s['SpawnCount'] for s in stages[sid]['Waves'][0]['MonsterSpawnInfos']})
        for enemy in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
            key=enemy.get_path_name()
            if key in seen:continue
            seen.add(key)
            eid=enemy.get_editor_property('character_tid')
            assert eid in expected, (sid,eid,expected)
            controller=enemy.get_controller()
            assert isinstance(controller,unreal.PGRoleAIController)
            controller.set_combat_thinking_enabled(False)
            observed[eid]+=1
            if eid in P09_IDS:
                appearance=enemy.appearance_component.get_appearance()
                mesh=enemy.appearance_component.get_presentation_mesh()
                assert appearance and mesh and mesh.get_editor_property('skeletal_mesh_asset')
                assert not mesh.get_editor_property('hidden_in_game')
                p09_seen.add(eid)
        assert all(observed[eid]<=count for eid,count in expected.items()), (sid,observed,expected)
        if observed!=expected:
            assert now-at<30, (sid,observed,expected)
            return
        assert manager.get_spawned_monsters()==sum(expected.values())
        assert manager.get_remaining_monsters()==sum(expected.values())
        report['waves'].append(dict(stage=sid,wave=1,spawned=dict(observed)))
        unreal.log('PGP09WaveProbe WAVE '+json.dumps(report['waves'][-1]))
        if sid==6:
            assert p09_seen==P09_IDS
            report['p09_models']=sorted(p09_seen)
            finish();return
        sid+=1;phase='start'
    except Exception:finish(traceback.format_exc())


handle=unreal.register_slate_post_tick_callback(tick)

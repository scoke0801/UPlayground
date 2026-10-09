"""Capture preparation, active frenzy, and boss HUD in an isolated PIE process."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import time
import traceback
import unreal

assert '-PGTestProfile=CombatHUD_' in unreal.SystemLibrary.get_command_line()
root=Path(unreal.Paths.project_dir()).resolve()
out=Path(os.environ.get('PG_COMBAT_HUD_OUTPUT',str(root/'Saved/CombatHUD'/('Presentation_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')))))
out.mkdir(parents=True,exist_ok=True)
report=dict(status='RUNNING',images=[],assisted=True)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/L_PG_ForestRuins')
unreal.InternationalizationLibrary.set_current_culture('ko',False)
assert unreal.PGEditorProbeTools.begin_play_window(int(os.environ.get('PG_COMBAT_HUD_WIDTH','1600')),int(os.environ.get('PG_COMBAT_HUD_HEIGHT','900')))
started=time.monotonic()
at=None
phase=0
stopped=None

def finish(error=None):
    global stopped
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('PGCombatHUD PRESENTATION '+report['status']+' '+str(out))
    level.editor_request_end_play()
    stopped=time.monotonic()

def capture(world,name):
    path=out/(name+'.png')
    assert unreal.PGEditorProbeTools.capture_game_viewport(world,str(path))
    assert path.is_file() and path.stat().st_size>10000
    report['images'].append(str(path))

def tick(_dt):
    global at,phase
    now=time.monotonic()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopped is not None:
        if not world and now-stopped>3:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    try:
        if now-started>180:raise RuntimeError('HUD presentation timeout')
        if not world:return
        player=unreal.GameplayStatics.get_player_pawn(world,0)
        if not player:return
        command=lambda text:unreal.SystemLibrary.execute_console_command(world,text)
        if at is None:
            command('t.MaxFPS 60');command('DisableAllScreenMessages')
            at=now;return
        if phase==0 and now-at>12:
            capture(world,'01_Preparation')
            command('PGBuildScenario Frenzy true')
            phase=1;at=now
        elif phase==1 and now-at>2:
            for _ in range(4):command('PGBuildProbe hit')
            phase=2;at=now
        elif phase==2 and now-at>.3:
            capture(world,'02_Frenzy')
            command('PGBossEncounter 15401')
            phase=3;at=now
        elif phase==3 and now-at>4:
            capture(world,'03_Boss')
            finish()
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)

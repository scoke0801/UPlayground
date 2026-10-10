"""Art-only rendered editor preview; no compilation, combat suite or profile writes."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
RUN=Path(os.environ['PG_DRESSING_RUN'])
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
report=dict(images=[],scope='Art-only PIE captures; no build or gameplay test')
u.EditorPythonScripting.set_keep_python_script_alive(True)
assert L.load_level('/Game/Maps/L_PG_ForestRuins')
u.InternationalizationLibrary.set_current_culture('ko',False)
assert u.PGEditorProbeTools.begin_play_window(1600,900)
start=time.monotonic()
ready=None
step=0
changed=0
camera=None
stopped=None

def capture(world,name):
    path=RUN/(name+'.png')
    assert u.PGEditorProbeTools.capture_game_viewport(world,str(path))
    report['images'].append(str(path))

def finish(error=None):
    global stopped
    report['status']='FAIL' if error else 'CAPTURED'
    if error:report['error']=error;u.log_error(error)
    (RUN/'preview.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    (ROOT/'Saved/LevelDressing/latest_preview.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    L.editor_request_end_play()
    stopped=time.monotonic()

def tick(dt):
    global ready,step,changed,camera
    now=time.monotonic()
    if stopped:
        if now-stopped>4 and not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
            camera=None
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()
        return
    try:
        if now-start>440:raise RuntimeError('Art capture timeout')
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world:return
        player=u.GameplayStatics.get_player_pawn(world,0)
        controller=u.GameplayStatics.get_player_controller(world,0)
        if not player or not controller:return
        if ready is None:
            ready=now;changed=now
            u.SystemLibrary.execute_console_command(world,'t.MaxFPS 45')
            u.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            return
        if now-changed<(35 if step==0 else 8):return
        if step==0:
            capture(world,'01_game_start')
            camera=u.PGEditorProbeTools.spawn_play_camera(world,u.Vector(-4800,5600,7400),u.Rotator())
            camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera.get_actor_location(),u.Vector(0,-300,200)),False)
            camera.camera_component.set_field_of_view(50)
            for widget in u.WidgetLibrary.get_all_widgets_of_class(world,u.UserWidget,True):widget.set_visibility(u.SlateVisibility.HIDDEN)
            controller.set_view_target_with_blend(camera,0.)
        elif step==1:
            capture(world,'02_overview')
            eye=u.Vector(-1600,600,2200)
            camera.set_actor_location(eye,False,False)
            camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye,u.Vector(650,-2400,440)),False)
            camera.camera_component.set_field_of_view(57)
        elif step==2:
            capture(world,'03_chapel')
            eye=u.Vector(0,0,7100)
            camera.set_actor_location(eye,False,False)
            camera.set_actor_rotation(u.Rotator(pitch=-90,yaw=-90),False)
            camera.camera_component.set_field_of_view(52)
        elif step==3:
            capture(world,'04_routes')
            player.set_actor_location(u.Vector(300,-1350,110),False,False)
            controller.set_view_target_with_blend(player,0.)
        elif step==4:
            capture(world,'05_chapel_game_camera')
            finish();return
        step+=1;changed=now
    except BaseException:finish(traceback.format_exc())

handle=u.register_slate_post_tick_callback(tick)

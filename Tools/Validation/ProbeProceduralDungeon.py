"""PIE navigation/regeneration acceptance probe; isolated profile supplied by runner."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
OUT=Path(os.environ.get('PG_DUNGEON_QA_DIR',str(ROOT/'Saved/QA/ProceduralDungeon')))
OUT.mkdir(parents=True,exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
RENDER='-PGDungeonRender' in u.SystemLibrary.get_command_line()
COUNT=1 if RENDER else 100
report=dict(status='RUNNING',seeds=[],images=[],direct_play=False)
u.EditorPythonScripting.set_keep_python_script_alive(True)
assert L.load_level('/Game/Maps/L_PG_ProceduralDungeon')
assert u.PGEditorProbeTools.begin_play_window(1600,900)
start=time.monotonic()
changed=start
index=0
camera=None
stopped=None
phase=0
def finish(error=None):
    global stopped,camera
    report['status']='FAIL' if error else 'PASS'
    if error: report['error']=error;u.log_error(error)
    (OUT/('render.json' if RENDER else 'runtime.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if not error:u.log('PGDungeon PROBE PASS')
    camera=None
    L.editor_request_end_play()
    stopped=time.monotonic()
def tick(dt):
    global changed,index,camera,phase
    now=time.monotonic()
    if stopped:
        if now-stopped>3 and not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
            u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.quit_editor()
        return
    try:
        if now-start>1100: raise RuntimeError('Probe timeout')
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world:return
        gs=u.GameplayStatics.get_all_actors_of_class(world,u.PGDungeonGenerator)
        if not gs:return
        g=gs[0]
        state=g.get_editor_property('state')
        if state==u.PGDungeonState.FAILED:raise RuntimeError(g.get_editor_property('last_error'))
        if state!=u.PGDungeonState.READY:return
        assert g.validate_navigation(),g.get_editor_property('last_error')
        components=g.get_components_by_class(u.InstancedStaticMeshComponent)
        definition=g.get_editor_property('definition')
        assert 8 <= len(components) <= 32, 'Unexpected component growth'
        assert len(g.get_editor_property('selected_modules')) == len(g.get_editor_property('layout').rooms)
        assert g.get_editor_property('decoration_point_count') > 0
        if not RENDER:
            if phase==1:
                report['recovery_after_cancel']='PASS'
                finish();return
            layout=g.get_editor_property('layout')
            report['seeds'].append(dict(seed=layout.seed,rooms=len(layout.rooms),attempt=layout.attempt,
                fallback=layout.fallback,seconds=round(now-changed,3),components=len(components)))
            index+=1
            if index>=COUNT:
                # Invalid definition, cancellation, and newer request must never publish stale Ready.
                g.set_editor_property('definition',None);g.generate(7)
                assert g.get_editor_property('state')==u.PGDungeonState.FAILED
                g.set_editor_property('definition',definition);g.generate(8);g.cancel_generation()
                assert g.get_editor_property('state')==u.PGDungeonState.CANCELLED
                assert len(g.get_components_by_class(u.InstancedStaticMeshComponent))==0
                report['failure_and_cancel']='PASS'
                phase=1;g.generate(-2147483648);changed=now
                return
            g.generate(index-1);changed=now
            return
        if now-changed<(30 if phase==0 else 5):return
        controller=u.GameplayStatics.get_player_controller(world,0)
        if phase==0:
            path=OUT/'01_quarter_view.png'
            assert u.PGEditorProbeTools.capture_game_viewport(world,str(path));report['images'].append(str(path))
            layout=g.get_editor_property('layout')
            xs=[r.cell.x*3800 for r in layout.rooms];ys=[r.cell.y*3800 for r in layout.rooms]
            target=u.Vector((min(xs)+max(xs))/2,(min(ys)+max(ys))/2,0)
            height=max(max(xs)-min(xs),max(ys)-min(ys))+5600
            eye=target+u.Vector(0,0,height*1.55)
            camera=u.PGEditorProbeTools.spawn_play_camera(world,eye,u.Rotator(pitch=-90,yaw=-90))
            camera.camera_component.set_field_of_view(65)
            controller.set_view_target_with_blend(camera,0.)
            for w in u.WidgetLibrary.get_all_widgets_of_class(world,u.UserWidget,True):w.set_visibility(u.SlateVisibility.HIDDEN)
        elif phase==1:
            path=OUT/'02_layout.png'
            assert u.PGEditorProbeTools.capture_game_viewport(world,str(path));report['images'].append(str(path))
            eye=u.Vector(-2000,2000,1800)
            camera.set_actor_location(eye,False,False)
            camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye,u.Vector(0,0,0)),False)
        elif phase==2:
            path=OUT/'03_room.png'
            assert u.PGEditorProbeTools.capture_game_viewport(world,str(path));report['images'].append(str(path))
            finish();return
        phase+=1;changed=now
    except BaseException:finish(traceback.format_exc())
handle=u.register_slate_post_tick_callback(tick)

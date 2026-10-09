"""Rendered PIE, real navigation/collision and assisted six-stage flow verification.

Uses an isolated test profile. Movement is injected through Pawn's movement input;
the six-stage smoke uses the existing assistance probe, not a manual play test.
"""
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/ForestRuins'
CONFIG=json.loads((ROOT/'Tools/Art/ForestRuins/layout.json').read_text(encoding='utf-8'))
RUN=OUT/'Preview'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
RUN.mkdir(parents=True)
REPORT=dict(status='RUNNING',run=str(RUN),map=CONFIG['map'],images=[],nav_paths=[],enemies={},stages=[],assisted=True)
ART_ONLY=os.environ.get('PG_FOREST_ART_ONLY')=='1'
REPORT['art_only']=ART_ONLY
REPORT['assisted']=not ART_ONLY
assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
LEVEL=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert LEVEL.load_level(CONFIG['map'])
unreal.InternationalizationLibrary.set_current_culture('ko',False)
assert unreal.PGEditorProbeTools.begin_play_window(1600,900)
STARTED=time.monotonic()
ready=None
phase='warmup'
phase_start=None
player=None
stage=None
controller=None
overview=None
wall_tests=[]
stopped=None

def vec(v): return [round(getattr(v,a),4) for a in ('x','y','z')]

def capture(world,name):
    path=RUN/(name+'.png')
    assert unreal.PGEditorProbeTools.capture_game_viewport(world,str(path)),name
    assert path.is_file() and path.stat().st_size>15000,name
    REPORT['images'].append(str(path))

def change(next_phase,now):
    global phase,phase_start
    phase,phase_start=next_phase,now

def finish(error=None):
    global stopped
    REPORT['status']='FAIL' if error else 'PASS'
    if error:REPORT['error']=error;unreal.log_error(error)
    (OUT/('art_preview.json' if ART_ONLY else 'preview.json')).write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    (RUN/'report.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    LEVEL.editor_request_end_play();stopped=time.monotonic()

def tick(_dt):
    global ready,player,stage,controller,overview,wall_tests,phase_start
    now=time.monotonic()
    if stopped is not None:
        if not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() and now-stopped>4:
            player=stage=controller=overview=None
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.SystemLibrary.quit_editor()
        return
    try:
        if now-STARTED>330: raise RuntimeError('Forest ruins PIE timeout in '+phase)
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world: return
        if ready is None:
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            stages=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGStageManager)
            if not player or not stages:return
            stage=stages[0];controller=unreal.GameplayStatics.get_player_controller(world,0)
            ready=now;phase_start=now
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            return
        if phase=='warmup':
            if now-ready<16:return
            position=player.get_actor_location()
            assert abs(position.z-110)<100,('Player did not settle on ground',vec(position))
            REPORT['player']=dict(cls=player.get_class().get_path_name(),location_cm=vec(position))
            capture(world,'01_게임_시작')
            if ART_ONLY:
                overview=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(-4800,5600,7400),unreal.Rotator())
                overview.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(overview.get_actor_location(),unreal.Vector(0,-150,150)),False)
                overview.camera_component.set_field_of_view(50)
                for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True):widget.set_visibility(unreal.SlateVisibility.HIDDEN)
                controller.set_view_target_with_blend(overview,0.)
                change('art_overview',now);return
            stage.ready_for_next_stage()
            unreal.SystemLibrary.execute_console_command(world,'PGCombatCycleSmoke')
            change('navigation',now);return
        if phase=='navigation':
            if now-phase_start<4:return
            REPORT['navigation_api']='PGEditorProbeTools.get_play_navigation_path_point_count (native complete-path query)'
            # Sample both rings and cross routes around the four combat islands.
            for x,y in [(-1950,-1450),(1950,-1450),(-1950,1450),(1950,1450),
                        (-1700,0),(1700,0),(0,-1500),(0,1500),(-700,-600),(700,600),(250,200),
                        (-1550,-850),(-1550,-150),(1550,250),(1550,950),(-550,1050),(1100,-1150)]:
                points=unreal.PGEditorProbeTools.get_play_navigation_path_point_count(world,unreal.Vector(x,y,10),unreal.Vector(0,0,10),player)
                assert points>=2,('No connected navigation route',x,y)
                REPORT['nav_paths'].append(dict(from_cm=[x,y],points=points))
            change('combat',now);return
        if phase=='combat':
            enemies=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy)
            for enemy in enemies:
                key=enemy.get_name()
                if key not in REPORT['enemies']:
                    REPORT['enemies'][key]=dict(start_cm=vec(enemy.get_actor_location()),last_cm=vec(enemy.get_actor_location()),max_speed=0.)
                row=REPORT['enemies'][key];row['last_cm']=vec(enemy.get_actor_location())
                row['max_speed']=max(row['max_speed'],enemy.get_velocity().length())
                points=unreal.PGEditorProbeTools.get_play_navigation_path_point_count(world,enemy.get_actor_location(),player.get_actor_location(),enemy)
                assert points>=2,('Enemy cannot reach player',key)
            if now-phase_start<7:return
            assert len(REPORT['enemies'])>=3,'No live enemy spawns'
            assert any(e['max_speed']>50 for e in REPORT['enemies'].values()),'Enemies did not move'
            capture(world,'02_실제_전투')
            # Camera actors exist only in PIE; authored level remains unchanged.
            overview=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(-4200,5000,6500),unreal.Rotator(pitch=-52,yaw=-50))
            overview.camera_component.set_field_of_view(48)
            controller.set_view_target_with_blend(overview,0.)
            change('overview',now);return
        if phase=='overview':
            if now-phase_start<4:return
            capture(world,'03_숲속_폐허_전체')
            overview.set_actor_location(unreal.Vector(0,0,7000),False,False)
            overview.set_actor_rotation(unreal.Rotator(pitch=-90,yaw=0),False)
            overview.camera_component.set_field_of_view(48)
            change('top',now);return
        if phase=='top':
            if now-phase_start<3:return
            capture(world,'04_전투_동선')
            controller.set_view_target_with_blend(player,0.)
            # Drive against four borders through actual CharacterMovement.
            wall_tests=[dict(name='서쪽',start=(-1950,0,110),direction=(-1,0,0),axis=0,sign=-1),
                        dict(name='동쪽',start=(1950,0,110),direction=(1,0,0),axis=0,sign=1),
                        dict(name='남쪽',start=(0,-1550,110),direction=(0,-1,0),axis=1,sign=-1),
                        dict(name='북쪽',start=(0,1550,110),direction=(0,1,0),axis=1,sign=1)]
            player.set_actor_location(unreal.Vector(*wall_tests[0]['start']),False,False)
            change('walls',now);return
        if phase=='walls':
            test=wall_tests[0]
            # The assisted cycle may open a reward modal during this test.
            # Force only bypasses its input gate; physical collision still applies.
            player.add_movement_input(unreal.Vector(*test['direction']),1.,True)
            if now-phase_start<2.5:return
            location=vec(player.get_actor_location());axis=test['axis']
            extent=CONFIG['boundary_half_extent_cm'][axis]
            assert extent-180<test['sign']*location[axis]<extent-35,('Border collision failed',test['name'],location)
            assert 30<location[2]<220,('Fell off ground',test['name'],location)
            REPORT.setdefault('wall_collision',[]).append(dict(side=test['name'],location_cm=location))
            wall_tests.pop(0)
            if wall_tests:
                player.set_actor_location(unreal.Vector(*wall_tests[0]['start']),False,False)
                change('walls',now)
            else:
                player.set_actor_location(unreal.Vector(-1500,800,110),False,False)
                change('edge',now)
            return
        if phase=='edge':
            if now-phase_start<3:return
            capture(world,'05_외곽_게임_시점')
            player.set_actor_location(unreal.Vector(*CONFIG['player_start_cm']),False,False)
            change('cycle',now);return
        if phase=='cycle':
            key=stage.get_current_stage_id()
            if key not in REPORT['stages']:REPORT['stages'].append(key)
            state=stage.get_current_stage_state()
            assert state!=unreal.PGStageState.FAILED,('Stage failed',key)
            if state==unreal.PGStageState.FINISHED:
                assert key==6,('Missing boss stage',key)
                # Let the existing cycle timer commit its completion log before exit.
                change('finished',now);return
        if phase=='finished':
            if now-phase_start<3:return
            capture(world,'06_시련_완료')
            REPORT['six_stage_flow']='PASS'
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True):
                widget.set_visibility(unreal.SlateVisibility.HIDDEN)
            overview.set_actor_location(unreal.Vector(-4800,5600,7400),False,False)
            overview.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(-4800,5600,7400),unreal.Vector(0,-150,150)),False)
            overview.camera_component.set_field_of_view(50)
            controller.set_view_target_with_blend(overview,0.)
            change('art_overview',now);return
        if phase=='art_overview':
            if now-phase_start<5:return
            capture(world,'07_환경_전체')
            eye=unreal.Vector(-1700,1200,1850)
            overview.set_actor_location(eye,False,False)
            overview.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye,unreal.Vector(550,-2100,290)),False)
            overview.camera_component.set_field_of_view(55)
            change('art_sanctuary',now);return
        if phase=='art_sanctuary':
            if now-phase_start<5:return
            capture(world,'08_성소_디테일');finish()
    except BaseException:finish(traceback.format_exc())

HANDLE=unreal.register_slate_post_tick_callback(tick)

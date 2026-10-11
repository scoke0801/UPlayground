"""Current 5/20/50-enemy PIE cost. Not a baseline comparison or packaged acceptance."""
import json, os, sys, time, traceback, math
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureMotionExpansion import TABLES, rows
from RunToonPerformance import csv_complete, metrics
OUT=Path(os.environ['PG_MOTION_RUN'])
assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
assert '-nullrhi' not in unreal.SystemLibrary.get_command_line(),'performance requires --render'
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
enemies={r['EnemyID']:r for r in rows(TABLES['enemies'])}
classes={eid:unreal.load_class(None,enemies[eid]['ActorClass']) for eid in [15204,15302,15303,15304,15501,15502,15503,15504]}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert level.load_level('/Game/Maps/RogueArena')
level.editor_request_begin_play();start=time.monotonic();stopped=None
phase='setup';at=0;index=0;population=[];player=manager=camera=None;counts=[5,20,50]
report=dict(status='RUNNING',phases=[],resolution=[1280,720],assisted=True,seed=173001,
    limitation='Current editor PIE only; no pre-change baseline, packaged 20-minute acceptance, or direct-input balance claim.')
def command(world,value):unreal.SystemLibrary.execute_console_command(world,value)
def finish(error=None):
    global stopped,player,manager,camera,population
    report['status']='FAIL' if error else 'CAPTURED'
    report['all_frame_p95_within_60fps_budget']=bool(report['phases']) and all(p['metrics']['FrameTime']['p95']<=1000./60 for p in report['phases'])
    if error:report['error']=error;unreal.log_error(error)
    else:unreal.log('PGMotionExpansion PASS')
    (OUT/'performance.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    population=[];player=manager=camera=None;level.editor_request_end_play();stopped=time.monotonic()
def tick(dt):
    global phase,at,index,population,player,manager,camera,path
    now=time.monotonic()
    if stopped:
        if now-stopped>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-start>240:finish('timeout '+phase);return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    try:
        if phase=='setup':
            if now-start<8:return
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if not player:return
            command(world,'PGStress 0 0');command(world,'t.MaxFPS 0');command(world,'r.VSync 0');command(world,'pg.AI.DebugDecisions 0')
            manager=unreal.GameplayStatics.get_actor_of_class(world,unreal.PGStageManager);manager.start_stage(1)
            at=now+25;phase='navigation';return
        if phase=='navigation':
            if manager.get_spawned_monsters()<5:
                assert now<at;return
            stage_actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy)
            for a in stage_actors:
                a.get_controller().set_combat_thinking_enabled(False);a.set_actor_hidden_in_game(True)
                a.set_actor_location(unreal.Vector(2500,2400,300),False,True)
            report['disabled_stage_actors']=len(stage_actors)
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(1000,-1300,1300),unreal.Rotator(-40,130,0))
            pos=player.get_actor_location();view=pos+unreal.Vector(1000,-1300,1300)
            camera.set_actor_location_and_rotation(view,unreal.MathLibrary.find_look_at_rotation(view,pos),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            phase='populate';return
        if phase=='populate':
            for a in population:a.destroy_actor()
            population=[];pos=player.get_actor_location()
            for n in range(counts[index]):
                eid=list(classes)[n%len(classes)];angle=n*2.399963;radius=350+70*(n%6)
                loc=pos+unreal.Vector(math.cos(angle)*radius,math.sin(angle)*radius,0)
                actor=unreal.PGEditorProbeTools.spawn_play_actor(world,classes[eid],unreal.Transform(location=loc))
                floor=pos.z-player.capsule_component.get_scaled_capsule_half_height()
                loc.z=floor+actor.capsule_component.get_scaled_capsule_half_height()+3
                actor.set_actor_location(loc,False,True);population.append(actor)
            at=now+5;phase='warmup';return
        if phase=='warmup':
            if now<at:return
            name='motion_'+OUT.name+'_'+str(counts[index])+'.csv';path=OUT/'User/Saved/Profiling/CSV'/name
            command(world,'CsvProfile STARTFILE='+name);command(world,'CsvProfile START')
            at=now+10;phase='capture';return
        if phase=='capture':
            if now<at:return
            command(world,'CsvProfile STOP');at=now+30;phase='flush';return
        if phase=='flush':
            if not csv_complete(path):
                assert now<at,'CSV not flushed';return
            sample=metrics(path);assert sample['FrameTime']['samples']>=100
            report['phases'].append(dict(authored_population=counts[index],live_enemies=len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy)),csv=str(path),metrics=sample,
                position_requests=sum(a.get_controller().get_position_move_count() for a in population)))
            index+=1
            if index<len(counts):phase='populate'
            else:finish()
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)

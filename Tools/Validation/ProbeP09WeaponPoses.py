"""Freeze source fractions on live P09 appearances and record the real weapon tip."""
import json, os, sys, time, traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureP09Weapons import SPEC, TABLES, rows
OUT=Path(os.environ['PG_P09_WEAPON_RUN'])
assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert level.load_level('/Game/Maps/RogueArena')
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
enemies={r['EnemyID']:r for r in rows(TABLES['enemies'])};skills={r['SkillID']:r for r in rows(TABLES['skills'])}
ids={'Spear':15705,'Greatsword':15711,'Staff':15717};placed=[]
for i,eid in enumerate(ids.values()):
    a=editor.spawn_actor_from_class(unreal.load_class(None,enemies[eid]['ActorClass']),unreal.Vector(1800,1300+i*200,200))
    a.set_editor_property('tags',['PGWeaponPose']);placed.append(a)
del a
cases=[(ids[s['family']],s['id'],i/20.) for s in SPEC['skills'] for i in range(1,20)]
unreal.EditorPythonScripting.set_keep_python_script_alive(True);level.editor_request_begin_play()
started=time.monotonic();stopped=None;phase='setup';index=0;at=0;actors={};camera=None;report=dict(status='RUNNING',samples=[])


def finish(error=None):
    global stopped,actors,camera
    if stopped:return
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    else:unreal.log('PGP09Weapons POSES PASS')
    (OUT/'poses.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    actors={};camera=None;level.editor_request_end_play();stopped=time.monotonic()


def tick(dt):
    global phase,index,at,actors,camera
    now=time.monotonic()
    if stopped:
        if now-stopped>4 and placed:
            for a in placed:editor.destroy_actor(a)
            placed.clear()
        if now-stopped>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>400:finish('pose timeout');return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    try:
        if phase=='setup':
            if now-started<8:return
            for cmd in ('t.MaxFPS 60','PGStress 0 0','DisableAllScreenMessages','r.MotionBlurQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,cmd)
            for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                a.get_controller().set_combat_thinking_enabled(False)
                if a.actor_has_tag('PGWeaponPose'):actors[a.get_editor_property('character_tid')]=a
            assert set(actors)==set(ids.values())
            for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if w.is_in_viewport():w.remove_from_parent()
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,unreal.Vector(),unreal.Rotator())
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            phase='pose';return
        if now<at:return
        eid,sid,fraction=cases[index];a=actors[eid]
        if phase=='pose':
            for other,instance in actors.items():instance.set_actor_hidden_in_game(other!=eid)
            a.set_actor_location_and_rotation(unreal.Vector(1400,1400,95),unreal.Rotator(),False,True)
            anim=a.mesh.get_anim_instance();anim.montage_stop(0.)
            montage=unreal.load_asset(skills[sid]['ElitePresentationMontage']);anim.montage_play(montage)
            anim.montage_set_position(montage,montage.get_play_length()*fraction);anim.montage_pause(montage)
            center=a.get_actor_location();loc=center+unreal.Vector(250,-410,220)
            camera.set_actor_location_and_rotation(loc,unreal.MathLibrary.find_look_at_rotation(loc,center),False,True)
            at=now+.22;phase='sample';return
        visual=a.appearance_component.get_presentation_mesh()
        gear=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==visual)
        tip=unreal.MathLibrary.transform_location(gear.get_world_transform(),unreal.Vector(0,-163 if eid==15705 else -162 if eid==15711 else -76,0))
        delta=tip-a.get_actor_location()
        report['samples'].append(dict(enemy=eid,skill=sid,fraction=fraction,tip=[delta.x,delta.y,delta.z],right_hand=str(visual.get_socket_location('hand_r')),left_hand=str(visual.get_socket_location('hand_l'))))
        if round(fraction*100)%10==0:
            assert unreal.PGEditorProbeTools.capture_game_viewport(world,(OUT/(str(sid)+'_'+str(round(fraction*100))+'.png')).as_posix())
        index+=1
        if index==len(cases):finish();return
        phase='pose';at=now+.05
    except Exception:finish(traceback.format_exc())


handle=unreal.register_slate_post_tick_callback(tick)

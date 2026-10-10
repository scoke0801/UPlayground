"""Compare live retargeted characters with original and candidate face materials."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=Path(os.environ['PG_TOON_UPGRADE_RUN'])
rows=json.loads((ROOT/'Saved/ToonImprovement/character-faces.json').read_text())['characters']
if '-PGP09Only' in unreal.SystemLibrary.get_command_line():rows=[r for r in rows if r['id'].startswith('P09')]
phases=[dict(row=row,candidate=candidate,angle=angle) for row in rows for angle in [70] for candidate in [False,True]]
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/L_PG_ForestRuins')
for i,row in enumerate(rows):
    if row['id'].startswith('P09'):
        cls=unreal.load_class(None,'/Game/DataCenter/Characters/BP_PGEnemy_'+row['id']+'.BP_PGEnemy_'+row['id']+'_C')
        actor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(cls,unreal.Vector(-650,200+i*220,150))
        actor.set_editor_property('tags',['PGFace_'+row['id']])
        actor.set_editor_property('auto_possess_ai',unreal.AutoPossessAI.DISABLED)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert unreal.PGEditorProbeTools.begin_play_window(1280,720)
started=time.monotonic();prepared=None;index=0;stopped=None;busy=False
report=dict(status='RUNNING',shots=[])
camera=None
subjects={}

def finish(error=None):
    global stopped
    report['status']='FAIL' if error else 'PASS'
    if error:report['error']=error;unreal.log_error(error)
    (OUT/'render.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global prepared,index,busy,camera
    if busy:return
    busy=True
    try:
        now=time.monotonic()
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if stopped:
            if not world and now-stopped>3:
                unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
            return
        if now-started>420:raise RuntimeError('Face comparison timeout')
        if not world or now-started<20:return
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        if not pawn or not pc:return
        if index==len(phases):finish();return
        phase=phases[index];row=phase['row']
        appearance=unreal.load_asset(row['appearance'])
        subject=pawn
        if row['id'].startswith('P09'):
            if row['id'] not in subjects:
                subject=unreal.GameplayStatics.get_all_actors_with_tag(world,'PGFace_'+row['id'])[0]
                subjects[row['id']]=subject
                if subject.get_controller():subject.get_controller().brain_component.stop_logic('Face presentation capture')
            subject=subjects[row['id']]
        component=subject.get_component_by_class(unreal.PGCharacterAppearanceComponent)
        if prepared is None:
            assert component.apply_appearance(appearance),row['id']+' appearance rejected'
            mesh=component.get_presentation_mesh()
            assert mesh.get_skeletal_mesh_asset()==appearance.mesh,row['id']+' wrong mesh'
            assert mesh and mesh.does_socket_exist(appearance.head_bone),row['id']
            mesh.set_material(row['index'],unreal.load_asset(row['candidate'] if phase['candidate'] else row['source']))
            for toon in subject.get_components_by_class(unreal.PGToonPresentationComponent):
                if toon.get_dynamic_material_count()==mesh.get_num_materials():
                    toon.initialize(mesh);break
            # Remains a meaningful analytic/SDF comparison after live adoption.
            # The original scalar/texture overrides are preserved by migration.
            material=mesh.get_material(row['index'])
            assert isinstance(material,unreal.MaterialInstanceDynamic)
            material.set_scalar_parameter_value('FaceSDFEnabled',1. if phase['candidate'] else 0.)
            for command in ['DisableAllScreenMessages','t.MaxFPS 60','r.ScreenPercentage 100','r.AntiAliasingMethod 4']:
                unreal.SystemLibrary.execute_console_command(world,command)
            prepared=now;return
        mesh=component.get_presentation_mesh()
        head=mesh.get_socket_transform(appearance.head_bone,unreal.RelativeTransformSpace.RTS_WORLD)
        head_forward=unreal.MathLibrary.transform_direction(head,appearance.head_forward_axis)
        forward=subject.get_actor_forward_vector()
        target=head.translation+unreal.Vector(0,0,4)
        location=target+forward*150+unreal.Vector(0,0,10)
        rotation=unreal.MathLibrary.find_look_at_rotation(location,target)
        if camera is None:
            camera=unreal.PGEditorProbeTools.spawn_play_camera(world,location,rotation)
            camera.camera_component.set_field_of_view(25)
        camera.set_actor_location_and_rotation(location,rotation,False,True)
        pc.set_view_target_with_blend(camera,0)
        lights=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DirectionalLight)
        key=max(lights,key=lambda a:a.light_component.intensity)
        yaw=unreal.MathLibrary.conv_vector_to_rotator(forward).yaw+180+phase['angle']
        key.set_actor_rotation(unreal.Rotator(pitch=-35,yaw=yaw,roll=0),False)
        if now-prepared<3:return
        name=f"{row['id']}_L{phase['angle']}_{'SDF' if phase['candidate'] else 'Original'}"
        path=OUT/(name+'.png')
        assert unreal.PGEditorProbeTools.capture_game_viewport(world,str(path)),name
        report['shots'].append(dict(id=row['id'],angle=phase['angle'],candidate=phase['candidate'],path=str(path),
            head_forward=str(head_forward),actor_forward=str(forward),head=str(head)))
        index+=1;prepared=None
    except BaseException:finish(traceback.format_exc())
    finally:busy=False

handle=unreal.register_slate_post_tick_callback(tick)

"""Read-only PIE grip capture: frozen authored poses, three close hand views."""
import json
from pathlib import Path
import re
import sys
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureMonsterVariations import rows,TABLES
from MonsterVariationRoster import P09_IDS,SPEC
command=unreal.SystemLibrary.get_command_line()
only=re.search(r'-PGGripEnemy=(\d+)',command)
if only:P09_IDS={int(only[1])}
assert '-PGTestProfile=' in command
OUT=Path(re.search(r'-PGGripEvidence="?([^"\s]+)',command)[1])
candidate=re.search(r'-PGGripCandidate="?([^"\s]+)',command)
overrides=json.loads(Path(candidate[1]).read_text(encoding='utf-8')) if candidate else {}
enemies={r['EnemyID']:r for r in rows(TABLES['enemies'])}
skills={r['SkillID']:r for r in rows(TABLES['skills'])}
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
placed=[]
for i,eid in enumerate(sorted(P09_IDS)):
    actor=editor.spawn_actor_from_class(unreal.load_class(None,enemies[eid]['ActorClass']),unreal.Vector(700+i*180,700,200))
    actor.set_editor_property('tags',['PGP09Grip'])
    actor.get_editor_property('character_movement').set_movement_mode(unreal.MovementMode.MOVE_NONE)
    placed.append(actor)
camera_placed=editor.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,0,800))
camera_placed.set_editor_property('tags',['PGP09GripCamera']);placed.append(camera_placed)
level.editor_request_begin_play()
started=time.monotonic();stopped=None;phase='setup';at=0.;index=0
actors={};camera=None
cases=[]
for eid in sorted(P09_IDS):
    poses=[(0,0)]+[(sid,f) for sid in enemies[eid]['SkillIdList'] for f in (.25,.45,.7)]
    for sid,fraction in poses:
        for bone in (['hand_r','hand_l'] if eid>=15205 else ['hand_r']):
            for view in ('outer','front','palm'):cases.append((eid,sid,fraction,bone,view))
report=dict(samples=[],direct_input=False,assets_modified=False)

def vector(v):return [v.x,v.y,v.z]
def transform(t):return dict(translation=vector(t.translation),rotation=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],scale=vector(t.scale3d))
def finish(error=None):
    global stopped,actors,camera
    if stopped:return
    report['status']='FAIL' if error else 'CAPTURED'
    report['max_contact_error_cm']=max((s.get('contact_error_cm',0.) for s in report['samples']),default=0.)
    report['all_hands_inside_arm_bounds']=bool(report['samples']) and all(s.get('hand_inside_arm_bounds',False) for s in report['samples'])
    if error:report['error']=error;unreal.log_error('PGP09Grip '+error)
    else:unreal.log('PGP09Grip CAPTURE PASS')
    (OUT/'poses.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    actors={};camera=None;level.editor_request_end_play();stopped=time.monotonic()

def tick(dt):
    global phase,at,index,actors,camera
    now=time.monotonic()
    if stopped:
        if now-stopped>4 and placed:
            for actor in placed:editor.destroy_actor(actor)
            placed.clear()
        if now-stopped>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    if now-started>290:finish('Timeout '+phase);return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    try:
        if phase=='setup':
            if now-started<8:return
            unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world,'PGStress 0 0')
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            unreal.SystemLibrary.execute_console_command(world,'r.MotionBlurQuality 0')
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
                if widget.is_in_viewport():widget.remove_from_parent()
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
                actor.get_controller().set_combat_thinking_enabled(False)
                if actor.actor_has_tag('PGP09Grip'):
                    actors[actor.get_editor_property('character_tid')]=actor
                    edited=actor.appearance_component.get_appearance()
                    defs=list(edited.get_editor_property('attachments'))
                    has_fingers=False
                    for attachment in defs:
                        key='Shield' if 'Shield' in attachment.mesh.get_path_name() else 'Sword'
                        values=overrides.get(str(actor.get_editor_property('character_tid')),{}).get(key,{})
                        if 'fingers' not in values:continue
                        fingers=[]
                        for value in values['fingers']:
                            finger=unreal.PGAppearanceGripFinger();finger.bone=value['bone']
                            pitch,yaw,roll=value['rotation'];finger.reference_rotation_offset=unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll)
                            fingers.append(finger)
                        attachment.set_editor_property('fingers',fingers);has_fingers=True
                    if has_fingers:
                        edited.set_editor_property('attachments',defs)
                        actor.appearance_component.apply_appearance(unreal.load_asset('/Game/DataCenter/Characters/DA_P09_Female'))
                        assert actor.appearance_component.apply_appearance(edited)
            assert set(actors)==P09_IDS
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if a.actor_has_tag('PGP09GripCamera'))
            camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(32.)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0.)
            phase='pose';at=now+2;return
        if now<at:return
        if phase=='done':finish();return
        eid,sid,fraction,bone,view=cases[index]
        actor=actors[eid];visual=actor.appearance_component.get_presentation_mesh();anim=actor.mesh.get_anim_instance()
        if phase=='pose':
            for other,instance in actors.items():instance.set_actor_hidden_in_game(other!=eid)
            actor.set_actor_location_and_rotation(unreal.Vector(1200,900,95),unreal.Rotator(),False,True)
            anim.montage_stop(0.)
            if sid:
                montage=unreal.load_asset(skills[sid]['ElitePresentationMontage'])
                anim.montage_play(montage)
                anim.montage_set_position(montage,montage.get_play_length()*fraction)
                anim.montage_pause(montage)
            for gear in actor.get_components_by_class(unreal.StaticMeshComponent):
                if gear.get_attach_parent()!=visual:continue
                key='Shield' if 'Shield' in gear.static_mesh.get_path_name() else 'Sword'
                value=overrides.get(str(eid),{}).get(key)
                if value:
                    t=unreal.Transform(location=unreal.Vector(*value['translation']),scale=unreal.Vector(*value['scale']))
                    pitch,yaw,roll=value['rotation'];t.rotation=unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll).quaternion()
                    gear.set_relative_transform(t,False,True)
            phase='camera';at=now+.3;return
        if phase=='camera':
            if index==0:
                report['components']=[dict(name=p.get_name(),mesh=p.get_skeletal_mesh_asset().get_path_name() if p.get_skeletal_mesh_asset() else None,materials=[p.get_material(i).get_path_name() if p.get_material(i) else None for i in range(p.get_num_materials())],visible=p.is_visible()) for p in actor.get_components_by_class(unreal.SkeletalMeshComponent)]
            hand=visual.get_socket_location(bone)
            gear=next(c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==visual and str(c.get_attach_socket_name())==bone)
            outward=hand-actor.get_actor_location();outward.z=0.;outward=outward.normal()
            direction=outward*65+unreal.Vector(15,0,20) if view=='outer' else unreal.Vector(60,-25 if bone=='hand_r' else 25,30)
            if view=='palm':
                ht=visual.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_WORLD)
                offset=unreal.Vector(.35,.10,.6) if bone=='hand_l' else unreal.Vector(0,0,-.65)
                direction=unreal.MathLibrary.transform_location(ht,offset)-hand
            target=hand+unreal.Vector(0,0,1)
            location=target+direction
            camera.set_actor_location_and_rotation(location,unreal.MathLibrary.find_look_at_rotation(location,target),False,True)
            names=[str(visual.get_bone_name(i)) for i in range(visual.get_num_bones())]
            report.setdefault('bone_names',names)
            side=bone[-1]
            bones={b:transform(visual.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_WORLD)) for b in names if b.lower()==bone or (b.lower().endswith('_'+side) and any(k in b.lower() for k in ('thumb','index','middle','ring','pinky','little')))}
            arm=next(p for p in actor.get_components_by_class(unreal.SkeletalMeshComponent) if p.get_skeletal_mesh_asset() and p.get_skeletal_mesh_asset().get_path_name().split('.')[0].endswith('_Arm'))
            origin,extent,radius=unreal.SystemLibrary.get_component_bounds(arm)
            contained=all(all(abs(t['translation'][i]-vector(origin)[i])<=vector(extent)[i]+.1 for i in range(3)) for t in bones.values())
            kind='Sword' if bone=='hand_r' else 'Shield'
            sex='Female' if eid%2 else 'Male'
            profile=SPEC['attachments'][kind]['profiles'][sex]
            actual_contact=unreal.MathLibrary.transform_location(gear.get_world_transform(),unreal.Vector(*profile['mesh_contact_cm']))
            hand_contact=unreal.MathLibrary.transform_location(visual.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_WORLD),unreal.Vector(*profile['hand_contact']))
            error=(actual_contact-hand_contact).length()
            report['samples'].append(dict(enemy=eid,skill=sid,fraction=fraction,bone=bone,view=view,hand=transform(visual.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_WORLD)),gear=transform(gear.get_world_transform()),bones=bones,relative=transform(gear.get_relative_transform()),contact_error_cm=error,hand_inside_arm_bounds=contained,arm_bounds=dict(origin=vector(origin),extent=vector(extent))))
            if not candidate:
                assert contained,(eid,sid,'Animated hand culled by arm bounds')
                assert error<.15,(eid,sid,'Grip contact error',error)
            phase='capture';at=now+.18;return
        if phase=='capture':
            filename=f'{eid}_{sid}_{round(fraction*100):02d}_{bone}_{view}.png'
            unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x960 filename="'+(OUT/filename).as_posix()+'"')
            index+=1
            if index==len(cases):phase='done'
            else:phase='pose'
            at=now+.22;return
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)

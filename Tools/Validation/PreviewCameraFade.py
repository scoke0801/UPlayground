"""Render the real player camera at visible/partial/hidden/restored distances in PIE."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

assert '-PGTestProfile=CameraFade' in unreal.SystemLibrary.get_command_line()
out = Path(os.environ['PG_CAMERA_FADE_RUN'])/'Presentation'
out.mkdir(parents=True, exist_ok=True)
settings = unreal.get_default_object(unreal.load_class(None, '/Script/PGData.PGCameraSettings'))
original = settings.get_editor_property('camera_mode')
settings.set_editor_property('camera_mode', next(getattr(unreal.PGCameraMode, n) for n in dir(unreal.PGCameraMode) if n.startswith('ACTION')))
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/L_PG_ForestRuins')
for label, tid, x in [('Enemy', 15205, -2000.), ('Creature', 15305, -3500.)]:
    cls = unreal.load_class(None, f'/Game/DataCenter/MonsterVariations/BP_{tid}.BP_{tid}_C')
    assert cls
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(cls, unreal.Vector(x, -300., 160.))
    actor.set_editor_property('tags', ['PGCameraFade'+label])
    actor.set_editor_property('auto_possess_ai', unreal.AutoPossessAI.DISABLED)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert unreal.PGEditorProbeTools.begin_play_window(1280, 720)
started = time.monotonic()
ready = None
stopped = None
busy = False
case = -1
at = 0
report = dict(status='RUNNING', cases=[])
cases = [('Visible', 240.), ('Fade25', 162.), ('Fade50', 137.), ('Fade75', 112.), ('Hidden', 60.), ('Restored', 240.)]
cases += [(prefix+name, distance) for prefix in ['Enemy', 'Creature'] for name, distance in list(cases)]

def tick(_dt):
    global ready, stopped, busy, case, at
    if busy: return
    busy = True
    try:
        now = time.monotonic()
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if stopped is not None:
            if not world and now - stopped > 5:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
                unreal.SystemLibrary.quit_editor()
            return
        try:
            assert now - started < 240, 'Camera fade preview timeout'
            player = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
            if not player: return
            if ready is None:
                for prefix in ['Enemy', 'Creature']:
                    enemies = unreal.GameplayStatics.get_all_actors_with_tag(world, 'PGCameraFade'+prefix)
                    assert len(enemies) == 1
                    enemies[0].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
                appearance = player.get_component_by_class(unreal.PGCharacterAppearanceComponent)
                asset = unreal.load_asset('/Game/DataCenter/Characters/DA_Bokusei')
                assert appearance and asset and appearance.apply_appearance(asset)
                assert appearance.get_presentation_mesh().get_skeletal_mesh_asset() == asset.mesh
                ready = now
                unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
                unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
                return
            if now - ready < 12: return
            camera = player.get_component_by_class(unreal.CameraComponent)
            capsule = player.get_component_by_class(unreal.CapsuleComponent)
            if case >= 0 and now - at < 3: return
            if case >= 0:
                name, distance = cases[case]
                subject = player
                for prefix in ['Enemy', 'Creature']:
                    if name.startswith(prefix):
                        subject = unreal.GameplayStatics.get_all_actors_with_tag(world, 'PGCameraFade'+prefix)[0]
                position = camera.get_world_location()
                actual = unreal.GameplayStatics.get_player_camera_manager(world, 0).get_camera_location()
                assert (actual - position).length() < 1., (actual, position)
                path = out/(name+'.png')
                assert unreal.PGEditorProbeTools.capture_game_viewport(world, str(path))
                meshes = []
                actors = [subject] + list(subject.get_attached_actors(reset_array=True, recursively_include_attached_actors=True))
                for actor in actors:
                    for mesh in actor.get_components_by_class(unreal.MeshComponent):
                        materials = [mesh.get_material(i) for i in range(mesh.get_num_materials())]
                        index = mesh.get_custom_primitive_data_index_for_scalar_parameter('CameraFadeAmount')
                        meshes.append(dict(mesh=mesh.get_name(), fade_data_index=index, materials=[m.get_path_name() for m in materials if m]))
                assert meshes
                if not name.startswith('Creature'):
                    presentation = subject.get_component_by_class(unreal.PGCharacterAppearanceComponent).get_presentation_mesh()
                    assert presentation.get_custom_primitive_data_index_for_scalar_parameter('CameraFadeAmount') == 7
                if subject == player:
                    weapons = [m for m in meshes if m['mesh'] == 'WeaponMesh' and m['materials']]
                    assert weapons and all(m['fade_data_index'] == 7 for m in weapons), weapons
                report['cases'].append(dict(name=name, distance=distance, camera=str(actual), image=str(path), meshes=meshes))
            case += 1
            if case < len(cases):
                # Move only the follow-camera in this unsaved test world; retain the real controller/view target.
                center = capsule.get_world_location()
                distance = cases[case][1]
                for prefix in ['Enemy', 'Creature']:
                    if cases[case][0].startswith(prefix):
                        subject = unreal.GameplayStatics.get_all_actors_with_tag(world, 'PGCameraFade'+prefix)[0]
                        subject.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
                        body = subject.get_component_by_class(unreal.CapsuleComponent)
                        center = body.get_world_location()
                        # Preserve the same surface clearance on different-sized monsters.
                        distance += body.get_scaled_capsule_radius() - capsule.get_scaled_capsule_radius()
                camera.set_absolute(False, True, False)
                # Rear-right view exposes the player's blade; rear-only captures can hide a missing sword.
                offset = unreal.Vector(-distance, 0., 20.) if case >= 6 else unreal.Vector(-distance * .5, distance * .8660254, 20.)
                location = center + offset
                camera.set_world_location_and_rotation(location,
                    unreal.MathLibrary.find_look_at_rotation(location, center + unreal.Vector(0., 0., 20.)), False, False)
                at = now
                return
            report['status'] = 'PASS'
        except Exception:
            report.update(status='FAIL', error=traceback.format_exc())
            unreal.log_error(report['error'])
        (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        settings.set_editor_property('camera_mode', original)
        level.editor_request_end_play()
        stopped = now
    finally:
        busy = False

handle = unreal.register_slate_post_tick_callback(tick)

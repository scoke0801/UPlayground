"""Actual player PIE: nine appearance swaps, SDF/head sync, shadow toggles/distance and cleanup."""
import json
import os
from pathlib import Path
import sys
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterCatalog import PLAYER_IDS

OUT = Path(os.environ['PG_PLAYABLE_TOON_RUN'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/L_PG_ForestRuins')
phases = [(identity, mode) for identity in PLAYER_IDS for mode in ['on', 'off']]
phases += [('Bokusei', mode) for mode in ['far', 'near', 'hidden', 'restored', 'reinitialize']]
report = dict(status='RUNNING', checks=[], shots=[])
started, prepared, stopped, index, busy = time.monotonic(), None, None, 0, False
camera, previous_proxy, previous_identity = None, None, None
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert unreal.PGEditorProbeTools.begin_play_window(1280, 720)


def finish(error=None):
    global stopped
    report['status'] = 'FAIL' if error else 'PASS'
    if error:
        report['error'] = error
        unreal.log_error(error)
    (OUT/'preview.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    level.editor_request_end_play()
    stopped = time.monotonic()


def tick(dt):
    global prepared, index, busy, camera, previous_proxy, previous_identity
    if busy:
        return
    busy = True
    try:
        now = time.monotonic()
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if stopped is not None:
            if not world and now-stopped > 3:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            return
        if now-started > 480:
            raise RuntimeError('Playable toon preview timeout')
        if not world or now-started < 20:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pawn or not pc:
            return
        if index == len(phases):
            finish()
            return
        identity, mode = phases[index]
        appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_'+identity)
        component = pawn.get_component_by_class(unreal.PGCharacterAppearanceComponent)
        assert component
        if prepared is None:
            if previous_identity and identity != previous_identity:
                # Rejected data must leave the previous visible mesh/proxy intact.
                original = dict(appearance.get_editor_property('material_overrides'))
                try:
                    invalid = dict(original)
                    invalid[9999] = next(iter(original.values()))
                    appearance.set_editor_property('material_overrides', invalid)
                    assert not component.apply_appearance(appearance)
                    assert component.get_appearance().id == previous_identity
                finally:
                    appearance.set_editor_property('material_overrides', original)
            assert component.apply_appearance(appearance), identity
            mesh = component.get_presentation_mesh()
            toons = pawn.get_components_by_class(unreal.PGToonPresentationComponent)
            assert len(toons) == 1, (identity, len(toons))
            toon = toons[0]
            proxy = toon.get_skeletal_hair_shadow_proxy()
            assert proxy and proxy.get_editor_property('leader_pose_component') == mesh
            if previous_identity != identity and previous_proxy:
                assert previous_proxy not in pawn.get_components_by_class(unreal.SkeletalMeshComponent), 'Leaked old proxy'
            assert component.apply_appearance(appearance) and toon.get_skeletal_hair_shadow_proxy() == proxy, 'Duplicate apply'
            if mode == 'reinitialize':
                old = proxy
                toon.initialize(mesh)
                proxy = toon.get_skeletal_hair_shadow_proxy()
                assert old not in pawn.get_components_by_class(unreal.SkeletalMeshComponent)
                assert proxy and proxy != old
                assert not isinstance(mesh.get_material(next(iter(appearance.get_editor_property('material_overrides')))).parent, unreal.MaterialInstanceDynamic)
            previous_proxy, previous_identity = proxy, identity
            toon.set_editor_property('hair_shadow_enabled', mode != 'off')
            pawn.set_actor_hidden_in_game(mode == 'hidden')
            pawn.set_actor_rotation(unreal.Rotator(yaw=25), False)
            for command in ['DisableAllScreenMessages', 't.MaxFPS 60', 'r.ScreenPercentage 100']:
                unreal.SystemLibrary.execute_console_command(world, command)
            key = toon.key_light
            assert key
            key.set_actor_rotation(unreal.Rotator(pitch=-35, yaw=205), False)
            prepared = now
            return
        mesh = component.get_presentation_mesh()
        toon = pawn.get_components_by_class(unreal.PGToonPresentationComponent)[0]
        proxy = toon.get_skeletal_hair_shadow_proxy()
        head = mesh.get_socket_transform(appearance.head_bone, unreal.RelativeTransformSpace.RTS_WORLD)
        target = head.translation + unreal.Vector(0, 0, 4)
        distance = appearance.hair_shadow_distance+500 if mode == 'far' else 160
        location = target + pawn.get_actor_forward_vector()*distance + unreal.Vector(0, 0, 8)
        rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
        if camera is None:
            camera = unreal.PGEditorProbeTools.spawn_play_camera(world, location, rotation)
            camera.camera_component.set_field_of_view(28)
        camera.set_actor_location_and_rotation(location, rotation, False, True)
        pc.set_view_target_with_blend(camera, 0)
        if now-prepared < 3:
            return
        assert bool(proxy.get_editor_property('cast_shadow')) == (mode not in ['off', 'far', 'hidden']), (identity, mode)
        assert proxy.get_editor_property('cast_hidden_shadow') and not proxy.get_editor_property('render_in_main_pass')
        assert proxy.get_skeletal_mesh_asset() == mesh.get_skeletal_mesh_asset()
        assert (proxy.get_socket_location(appearance.head_bone)-head.translation).length() < .01, 'Hair pose mismatch'
        for slot, expected in appearance.get_editor_property('material_overrides').items():
            material = mesh.get_material(slot)
            assert isinstance(material, unreal.MaterialInstanceDynamic) and material.parent == expected
            assert material.get_scalar_parameter_value('FaceSDFEnabled') == 1.
            direction = material.get_vector_parameter_value('HeadForwardWS')
            actual = unreal.Vector(direction.r, direction.g, direction.b)
            expected_direction = unreal.MathLibrary.transform_direction(head, appearance.head_forward_axis)
            expected_direction = expected_direction / expected_direction.length()
            assert (actual-expected_direction).length() < .001, 'SDF head frame mismatch'
        report['checks'].append(dict(id=identity, mode=mode, status='PASS'))
        if mode in ['on', 'off']:
            path = OUT/(identity+'_'+mode+'.png')
            assert unreal.PGEditorProbeTools.capture_game_viewport(world, str(path))
            report['shots'].append(str(path))
        index += 1
        prepared = None
    except BaseException:
        finish(traceback.format_exc())
    finally:
        busy = False


handle = unreal.register_slate_post_tick_callback(tick)

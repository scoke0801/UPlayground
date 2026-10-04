"""Build a saved, isolated six-motion gallery and capture fresh SM6 frames.

Run in UnrealEditor (not the NullRHI commandlet). Normal game maps/player assets
are untouched. Saved map actors loop sequences in PIE; editor captures freeze at
deterministic sample times. Outline followers use leader pose (no duplicate eval).
"""
import json
import math
import shutil
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
AUDIT = json.loads((ROOT / 'Saved/BokuseiMotion/configure.json').read_text(encoding='utf-8'))
assert AUDIT['status'] == 'PASS'
OUT = ROOT / 'Saved/BokuseiMotion/Preview' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
LATEST = ROOT / 'Saved/BokuseiMotion/preview.json'
MAP = '/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_MotionTest'
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'map': MAP, 'images': []}
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
mesh = unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
outline_slots = [{'asset':'/Game/Art/ToonTest/Bokusei/Materials/MI_PGToonOutline_Bokusei_'+str(slot.material_slot_name)} for slot in mesh.get_editor_property('materials')]
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
assert world
world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
volume.set_actor_label('PG Toon Fixed Exposure')
volume.set_editor_property('unbound', True)
settings = unreal.PostProcessSettings()
for key, value in {
    'override_auto_exposure_method': True,
    'auto_exposure_method': unreal.AutoExposureMethod.AEM_MANUAL,
    'override_auto_exposure_apply_physical_camera_exposure': True,
    'auto_exposure_apply_physical_camera_exposure': False,
    'override_auto_exposure_bias': True,
    'auto_exposure_bias': 0.0,
    'override_bloom_intensity': True,
    'bloom_intensity': 0.0,
}.items():
    settings.set_editor_property(key, value)
volume.set_editor_property('settings', settings)
components = []
outlines = []
locations = []
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def stage_material():
    path = '/Game/Art/ToonTest/Materials/M_PGToonPreviewFloor'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = TOOLS.create_asset('M_PGToonPreviewFloor', '/Game/Art/ToonTest/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, 0, 0)
    color.set_editor_property('constant', unreal.LinearColor(.045, .06, .095, 1))
    unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -8))
floor.set_actor_label('PG Toon Preview Floor')
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.static_mesh_component.set_material(0, stage_material())
floor.set_actor_scale3d(unreal.Vector(13, 11, .1))
label_light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 250), unreal.Rotator(pitch=-55, yaw=-45, roll=0))
label_light.set_actor_label('PG Toon Label Light (unlit characters unaffected)')
label_light.light_component.set_editor_property('intensity', 3.0)
label_light.light_component.set_editor_property('cast_shadows', False)

for index, entry in enumerate(AUDIT['clips']):
    right = (index % 3 - 1) * 285
    away = (index // 3 - .5) * 260
    pos = unreal.Vector((right+away)/math.sqrt(2), (right-away)/math.sqrt(2), 0)
    locations.append(pos)
    base = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, pos)
    base.set_actor_label('PG Toon ' + entry['label'])
    comp = base.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(mesh)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    anim = unreal.load_asset(entry['asset'])
    play_data = unreal.SingleAnimationPlayData()
    play_data.set_editor_property('anim_to_play', anim)
    play_data.set_editor_property('saved_looping', True)
    play_data.set_editor_property('saved_playing', True)
    play_data.set_editor_property('saved_position', entry['duration'] * entry['sample_fraction'])
    comp.set_editor_property('animation_data', play_data)
    comp.set_animation(anim)
    comp.set_position(entry['duration'] * entry['sample_fraction'], False)
    comp.set_update_animation_in_editor(True)
    comp.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    comp.set_editor_property('cast_shadow', False)
    comp.set_play_rate(0)
    components.append(comp)
    hull = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, pos)
    hull.set_actor_label('PG Toon ' + entry['label'] + ' Outline')
    outlines.append(hull)
    hull_comp = hull.skeletal_mesh_component
    hull_comp.set_skeletal_mesh_asset(mesh)
    hull_comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    hull_comp.set_editor_property('cast_shadow', False)
    hull_comp.set_leader_pose_component(comp)
    for slot in range(hull_comp.get_num_materials()):
        hull_comp.set_material(slot, unreal.load_asset(outline_slots[slot]['asset']))
    label = actors.spawn_actor_from_class(unreal.TextRenderActor, pos + unreal.Vector(-75, 75, 5), unreal.Rotator(pitch=55, yaw=135, roll=0))
    label.set_actor_label('PG Toon Label ' + entry['label'])
    label.text_render.set_text(entry['label'])
    label.text_render.set_world_size(23)
    label.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)

target = unreal.Vector(0, 0, 55)
rotation = unreal.Rotator(pitch=-55, yaw=-45, roll=0)
camera_location = target - unreal.MathLibrary.get_forward_vector(rotation) * 1200
camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_location, rotation)
camera.set_actor_label('PG Toon Quarter View 1200cm')
camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
camera.camera_component.set_editor_property('field_of_view', 60)
level.set_level_viewport_camera_info(camera_location, rotation, 'None')
level.editor_set_game_view(True)
level.set_level_viewport_fov(60, 'None')
REPORT['camera_rotation'] = {'pitch': rotation.pitch, 'yaw': rotation.yaw, 'roll': rotation.roll}
REPORT['camera_distance_cm'] = 1200
# Protect an existing generated test map on reruns.
map_file = ROOT / 'Content/Art/ToonTest/Maps/L_PGToon_Bokusei_MotionTest.umap'
if map_file.is_file():
    shutil.copy2(map_file, OUT / 'previous_map.umap')
# Save looping playback settings; snapshots below only affect this editor session.
for comp in components:
    comp.set_play_rate(1)
assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
for comp in components:
    comp.set_play_rate(0)

for command in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.Streaming.FullyLoadUsedTextures 1', 'viewmode lit']:
    unreal.SystemLibrary.execute_console_command(world, command)
shots = [(AUDIT['clips'][i]['label'] + '_Close', i, AUDIT['clips'][i]['sample_fraction']) for i in range(6)]
shots += [('Attack_Early', 2, .15), ('Attack_Late', 2, .7), ('QuarterView_AllMotions', None, None)]
started = time.monotonic()
last = started
index = 0
pending = None
finished = False
pie_started = None
pie_first = None
pie_bones_first = None
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def finish(error=None):
    global finished
    if finished:
        return
    finished = True
    REPORT['status'] = 'FAIL' if error else 'CAPTURED'
    REPORT['visual_review_required'] = True
    if error:
        REPORT['error'] = error
        unreal.log_error(error)
    payload = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (OUT / 'preview.json').write_text(payload, encoding='utf-8')
    LATEST.write_text(payload, encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    if pie_started:
        level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global last, index, pending, pie_started, pie_first, pie_bones_first
    try:
        now = time.monotonic()
        if now-started > 240:
            finish('Render timeout')
            return
        if pie_started:
            game_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not game_world or now-pie_started < 1:
                return
            game_actors = unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.SkeletalMeshActor)
            bases = [a.skeletal_mesh_component for a in game_actors if not a.get_actor_label().endswith(' Outline')]
            hulls = [a.skeletal_mesh_component for a in game_actors if a.get_actor_label().endswith(' Outline')]
            assert len(bases) == 6 and len(hulls) == 6
            samples = {str(c.get_outer().get_actor_label()): c.get_position() for c in bases}
            assert all(c.is_playing() and abs(c.get_play_rate()-1) < .001 for c in bases)
            for c in hulls:
                leader = c.get_editor_property('leader_pose_component')
                assert leader and leader.get_world() == game_world, 'PIE follower lost leader'
            bone_samples = {c.get_outer().get_actor_label(): {b: [c.get_socket_location(b).x,c.get_socket_location(b).y,c.get_socket_location(b).z] for b in ['Head','Hand_L','Hand_R','Foot_L','Foot_R']} for c in bases}
            if pie_first is None:
                pie_bones_first = bone_samples
                pie_first = samples
                last = now
                return
            if now-last < .31:
                return
            assert all(abs(samples[k]-pie_first[k]) > .001 for k in samples), 'PIE animation did not advance'
            movement = {key:max(math.dist(value[b],pie_bones_first[key][b]) for b in value) for key,value in bone_samples.items()}
            assert all(v > .001 for v in movement.values()), ('PIE pose did not move',movement)
            REPORT['pie_bone_movement_cm'] = movement
            REPORT['pie_playback'] = {'status': 'PASS', 'first_positions': pie_first, 'second_positions': samples,
                                      'playing_actors': 6, 'outline_leaders': 6}
            finish()
            return
        if now-started < 20 or now-last < 3:
            return
        if pending:
            if not pending.is_file() or pending.stat().st_size < 10000:
                return
            REPORT['images'].append(str(pending))
            pending = None
            index += 1
            last = now
            if index >= len(shots):
                level.set_level_viewport_camera_info(camera_location, rotation, 'None')
                for comp, clip in zip(components, AUDIT['clips']):
                    comp.override_animation_data(unreal.load_asset(clip['asset']), True, True,
                                                 clip['duration'] * clip['sample_fraction'], 1)
                pie_started = now
                level.editor_request_begin_play()
            return
        name, actor_index, fraction = shots[index]
        # SetPosition alone only changes the animation clock in a paused editor.
        # OverrideAnimationData explicitly evaluates and refreshes bone transforms.
        errors = []
        for i, (comp, clip) in enumerate(zip(components, AUDIT['clips'])):
            sample = fraction if i == actor_index else clip['sample_fraction']
            anim = unreal.load_asset(clip['asset'])
            comp.set_position(clip['duration'] * sample, False)
            comp.override_animation_data(anim, True, False, clip['duration'] * sample, 0)
            pose = anim.get_anim_pose_at_time(clip['duration'] * sample, unreal.AnimPoseEvaluationOptions())
            origin = comp.get_outer().get_actor_location()
            for bone in ['Head', 'Hand_L', 'Hand_R', 'Foot_L', 'Foot_R']:
                expected = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
                actual = comp.get_socket_location(bone) - origin
                errors.append(math.dist([expected.x,expected.y,expected.z],[actual.x,actual.y,actual.z]))
        assert max(errors) < .1, ('Frozen capture pose mismatch',name,max(errors))
        REPORT.setdefault('capture_pose_checks', []).append(dict(shot=name, max_error_cm=max(errors), fraction=fraction))
        for i, (comp, hull) in enumerate(zip(components, outlines)):
            hidden = actor_index is not None and i != actor_index
            comp.get_outer().set_is_temporarily_hidden_in_editor(hidden)
            hull.set_is_temporarily_hidden_in_editor(hidden)
        if actor_index is None:
            location, rot = camera_location, rotation
        else:
            aim = locations[actor_index] + unreal.Vector(0, 0, 85)
            location = aim + unreal.Vector(-135, 350, 45)
            if actor_index == 5:
                aim = locations[actor_index] + unreal.Vector(0, -75, 35)
                location = aim + unreal.Vector(300, 120, 170)
            rot = unreal.MathLibrary.find_look_at_rotation(location, aim)
        level.set_level_viewport_camera_info(location, rot, 'None')
        pending = OUT / (name + '.png')
        unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x720 filename="{pending.as_posix()}"')
        last = now
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

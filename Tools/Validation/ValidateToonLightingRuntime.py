"""Rendered motion, runtime parameter/lifecycle and editor PIE CSV characterization.

Run with -csvGpuStats. This is an isolated art fixture, not a packaged game benchmark.
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
DATA = json.loads((ROOT / 'Saved/ToonTest/lighting_lab.json').read_text())
CLIPS = json.loads((ROOT / 'Saved/ToonTest/retarget.json').read_text())['clips']
ROW = next(row for row in DATA['characters'] if row['name'] == 'Inori')
RUN = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT = ROOT / 'Saved/ToonTest/Runtime' / RUN
OUT.mkdir(parents=True)
LATEST = ROOT / 'Saved/ToonTest/lighting_runtime.json'
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'images': [], 'phases': [],
          'limits': ['Editor PIE art fixture, no gameplay/AI; not packaged performance',
                     'CSV includes editor overhead; fixed 1280x720 requested, viewport size recorded']}
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
# Offscreen PIE must not enter the editor's 3 fps background throttle.
performance_settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
previous_throttle = performance_settings.get_editor_property('bThrottleCPUWhenNotForeground')
performance_settings.set_editor_property('bThrottleCPUWhenNotForeground', False)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Art/ToonTest/Maps/L_PGToon_LightingLab')
existing = actors.get_all_level_actors()
template = next(a for a in existing if a.get_actor_label() == 'PG Toon Lab Inori')
AXES = {k: template.toon_presentation.get_editor_property(k) for k in ['head_bone', 'head_forward_axis', 'head_right_axis']}
key = next(a for a in existing if a.get_actor_label() == 'PG Toon Key Light')
camera = next(a for a in existing if isinstance(a, unreal.CameraActor))
for a in existing:
    if isinstance(a, unreal.PGToonPreviewActor):
        actors.destroy_actor(a)
mesh = unreal.load_asset(ROW['mesh'])
materials = [unreal.load_asset(r['asset']) for r in ROW['materials']]


def configure(actor, clip, key_light):
    c = actor.skeletal_mesh_component
    c.set_skeletal_mesh_asset(mesh)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_render_custom_depth(True)
    c.set_custom_depth_stencil_value(73)
    for i, mi in enumerate(materials):
        c.set_material(i, mi)
    for k, v in AXES.items():
        actor.toon_presentation.set_editor_property(k, v)
    actor.toon_presentation.set_editor_property('key_light', key_light)
    data = unreal.SingleAnimationPlayData()
    data.anim_to_play = unreal.load_asset(clip['asset'])
    data.saved_looping = data.saved_playing = True
    data.saved_position = clip['duration'] * clip['sample_fraction']
    c.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    c.set_editor_property('animation_data', data)
    c.set_animation(data.anim_to_play)
    c.set_position(data.saved_position, False)
    c.set_update_animation_in_editor(True)
    c.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    c.set_play_rate(1)
    return actor


bases = []
for i, clip in enumerate(CLIPS):
    a = actors.spawn_actor_from_class(unreal.PGToonPreviewActor, unreal.Vector((i%3-1)*260, (i//3-.5)*240, 0))
    a.set_actor_label('PG Toon Motion '+clip['label'])
    configure(a, clip, key)
    bases.append(a)
loc, aim = unreal.Vector(0, 1050, 950), unreal.Vector(0, 0, 65)
rot = unreal.MathLibrary.find_look_at_rotation(loc, aim)
camera.set_actor_location(loc, False, False)
camera.set_actor_rotation(rot, False)
camera.camera_component.set_editor_property('field_of_view', 50)
MAP = '/Game/Art/ToonTest/Maps/L_PGToon_AdvancedMotionTest'
old = ROOT / 'Content/Art/ToonTest/Maps/L_PGToon_AdvancedMotionTest.umap'
if old.exists():
    shutil.copy2(old, OUT / 'previous_map.umap')
assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
REPORT['map'] = MAP
for a in bases:
    a.toon_presentation.initialize(a.skeletal_mesh_component)
    a.skeletal_mesh_component.set_play_rate(0)
level.editor_set_game_view(True)
level.set_level_viewport_camera_info(loc, rot, 'None')
level.set_level_viewport_fov(50, 'None')
for command in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.VSync 0', 't.MaxFPS 0', 'r.Streaming.FullyLoadUsedTextures 1']:
    unreal.SystemLibrary.execute_console_command(world, command)
shots = [('SixMotions', None, 100), ('Attack_100', 3, 100), ('Attack_50', 3, 50), ('Death', 5, 100)]
phases = [('one_outline', 1, True), ('ten_outline', 10, True), ('fifty_outline', 50, True),
          ('fifty_no_outline', 50, False), ('fifty_outline_repeat', 50, True), ('one_return', 1, True)]
started = last = time.monotonic()
shot_index, pending, prepared = 0, None, False
pie_started, first_sample, phase_start, orbit_start = None, None, None, None
orbit_images = []
game_world, game_key, game_pp = None, None, None
game_bases = []
phase_index, capturing, capture_started = 0, False, None
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def console(command):
    unreal.SystemLibrary.execute_console_command(game_world or world, command)


def finish(error=None):
    if capturing:
        console('CsvProfile STOP')
    REPORT.update(status='FAIL' if error else 'PASS', visual_review_required=True)
    if error:
        REPORT['error'] = error
        unreal.log_error(error)
    payload = json.dumps(REPORT, indent=2)
    LATEST.write_text(payload, encoding='utf-8')
    (OUT / 'runtime.json').write_text(payload, encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    performance_settings.set_editor_property('bThrottleCPUWhenNotForeground', previous_throttle)
    if pie_started:
        if game_world:
            unreal.PGToonPreviewActor.set_preview_viewport_size(game_world, 0, 0)
        level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def vector_values(v):
    return [v.x, v.y, v.z]


def assert_vector(color, vec):
    assert all(abs(a-b) < .003 for a, b in zip([color.r, color.g, color.b], vector_values(vec))), (color, vec)


def runtime_samples():
    result = {}
    for a in game_bases:
        c, p = a.skeletal_mesh_component, a.toon_presentation
        assert p.get_dynamic_material_count() == len(materials)
        mids = [c.get_material(i) for i in range(c.get_num_materials())]
        assert all(isinstance(mi, unreal.MaterialInstanceDynamic) for mi in mids)
        assert c.is_playing() and c.get_play_rate() == 1
        expected = unreal.MathLibrary.transform_direction(c.get_socket_transform(p.head_bone), p.head_forward_axis)
        faces = [mi for mi in mids if mi.get_scalar_parameter_value('FaceShading') > .01]
        assert faces
        for mi in faces:
            assert_vector(mi.get_vector_parameter_value('HeadForwardWS'), expected)
        assert_vector(mids[0].get_vector_parameter_value('LightDirection'), game_key.get_actor_forward_vector())
        result[a.get_actor_label()] = {'position': c.get_position(), 'forward': vector_values(expected)}
    return result


def spawn_runtime(i):
    transform = unreal.Transform(location=unreal.Vector((i%10-4.5)*110, (i//10-2)*135, 0))
    a = unreal.PGToonPreviewActor.spawn_preview_actor(game_world, transform)
    assert a
    configure(a, CLIPS[i % len(CLIPS)], game_key)
    a.toon_presentation.initialize(a.skeletal_mesh_component)
    a.skeletal_mesh_component.play(True)
    return a


def lifecycle():
    a = game_bases[0]
    c, p = a.skeletal_mesh_component, a.toon_presentation
    for _ in range(20):
        p.initialize(None)
        assert p.get_dynamic_material_count() == 0
        assert all(c.get_material(i) == m for i, m in enumerate(materials))
        p.initialize(c)
        p.initialize(c)
        assert all(c.get_material(i).get_editor_property('parent') == m for i, m in enumerate(materials))
    replacement = materials[1]
    c.set_material(0, replacement)
    p.initialize(None)
    assert c.get_material(0) == replacement, 'Restore overwrote external material owner'
    c.set_material(0, materials[0])
    p.set_editor_property('key_light', None)
    p.set_editor_property('head_bone', 'MissingBoneForFallbackTest')
    p.initialize(c)
    v = c.get_material(0).get_vector_parameter_value('LightDirection')
    assert all(math.isfinite(x) for x in [v.r, v.g, v.b])
    assert abs(v.r*v.r+v.g*v.g+v.b*v.b-1) < .001
    for k, v in AXES.items():
        p.set_editor_property(k, v)
    p.set_editor_property('key_light', game_key)
    p.refresh_presentation()
    before = len(unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.PGToonPreviewActor))
    for i in range(20):
        temp = spawn_runtime(i)
        assert temp.toon_presentation.get_dynamic_material_count() == len(materials)
        temp.destroy_actor()
    after = len(unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.PGToonPreviewActor))
    assert before == after
    REPORT['lifecycle'] = {'reinitialize_cycles': 20, 'spawn_destroy_cycles': 20,
                           'external_override_preserved': True, 'missing_light_bone_fallback': True,
                           'actors_before': before, 'actors_after': after}


def set_population(count, outline):
    while len(game_bases) > count:
        game_bases.pop().destroy_actor()
    while len(game_bases) < count:
        game_bases.append(spawn_runtime(len(game_bases)))
    for i, a in enumerate(game_bases):
        a.set_actor_location(unreal.Vector((i%10-4.5)*110, (i//10-2)*135, 0), False, False)
        a.skeletal_mesh_component.set_render_custom_depth(outline)
    game_pp.add_or_update_blendable(unreal.load_asset(DATA['outline']), 1. if outline else 0.)
    assert len(unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.PGToonPreviewActor)) == count


def tick(_dt):
    global last, shot_index, pending, prepared, pie_started, first_sample
    global game_world, game_key, game_pp, game_bases, phase_start, phase_index, capturing, orbit_start, capture_started
    try:
        now = time.monotonic()
        if now-started > 420:
            finish('Runtime validation timeout')
            return
        if pie_started:
            if game_world is None:
                game_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
                if not game_world or now-pie_started < 2:
                    game_world = None
                    return
                game_bases = list(unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.PGToonPreviewActor))
                game_key = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.DirectionalLight) if a.get_actor_label() == 'PG Toon Key Light')
                game_pp = unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.PostProcessVolume)[0]
                assert len(game_bases) == 6
                assert not unreal.GameplayStatics.get_player_pawn(game_world, 0), 'Art fixture spawned a default pawn'
                assert unreal.PGToonPreviewActor.set_preview_viewport_size(game_world, 1280, 720)
                first_sample = runtime_samples()
                game_key.set_actor_rotation(unreal.Rotator(pitch=-40, yaw=-130, roll=0), False)
                REPORT['render_settings'] = {k: unreal.SystemLibrary.get_console_variable_float_value(k) for k in
                    ['r.ScreenPercentage', 'r.VSync', 't.MaxFPS', 'sg.ShadowQuality', 'sg.PostProcessQuality']}
                REPORT['render_settings']['background_throttle'] = False
                pc = unreal.GameplayStatics.get_player_controller(game_world, 0)
                REPORT['viewport_size'] = list(pc.get_viewport_size())
                assert REPORT['viewport_size'] == [1280, 720], REPORT['viewport_size']
                last = now
                return
            if phase_start is None:
                if orbit_start is not None:
                    elapsed = now-orbit_start
                    game_camera = unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.CameraActor)[0]
                    angle = math.radians(-25 + min(elapsed, 8)*7)
                    orbit_loc = unreal.Vector(math.sin(angle)*1050, math.cos(angle)*1050, 950)
                    game_camera.set_actor_location(orbit_loc, False, False)
                    game_camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(orbit_loc, aim), False)
                    game_key.set_actor_rotation(unreal.Rotator(pitch=-40, yaw=-150+min(elapsed, 8)*12, roll=0), False)
                    if len(orbit_images) < 4 and elapsed >= 1+len(orbit_images)*2:
                        path = OUT / ('Orbit_'+str(len(orbit_images))+'.png')
                        console(f'HighResShot 1280x720 filename="{path.as_posix()}"')
                        orbit_images.append(path)
                    if elapsed < 12:
                        return
                    assert all(p.is_file() and p.stat().st_size > 10000 for p in orbit_images)
                    REPORT['images'] += [str(p) for p in orbit_images]
                    REPORT['continuous_camera_light_motion_seconds'] = 8
                    game_camera.set_actor_location(loc, False, False)
                    game_camera.set_actor_rotation(rot, False)
                    game_key.set_actor_rotation(unreal.Rotator(pitch=-55, yaw=-90, roll=0), False)
                    set_population(phases[0][1], phases[0][2])
                    phase_start = now
                    return
                if now-last < 1:
                    return
                second = runtime_samples()
                assert all(abs(first_sample[k]['position']-v['position']) > .001 for k, v in second.items())
                assert any(sum(abs(a-b) for a,b in zip(first_sample[k]['forward'],v['forward'])) > .001 for k,v in second.items())
                REPORT['animated_head_and_light'] = {'status': 'PASS', 'first': first_sample, 'second': second}
                lifecycle()
                orbit_start = now
                return
            label, count, outline = phases[phase_index]
            elapsed = now-phase_start
            if not capturing and elapsed >= 8:
                assert list(unreal.GameplayStatics.get_player_controller(game_world, 0).get_viewport_size()) == [1280, 720]
                console('CsvProfile STARTFILE=toon_'+RUN+'_'+label+'.csv')
                console('CsvProfile START')
                capturing = True
                capture_started = now
            if capturing and now-capture_started >= 12:
                console('CsvProfile STOP')
                capturing = False
                REPORT['phases'].append({'name': label, 'characters': count, 'outline': outline,
                    'csv': str(ROOT/'Saved/Profiling/CSV'/('toon_'+RUN+'_'+label+'.csv')), 'sample_seconds': now-capture_started})
                phase_index += 1
                if phase_index == len(phases):
                    finish()
                    return
                set_population(phases[phase_index][1], phases[phase_index][2])
                phase_start = now
            return
        if now-started < 35 or now-last < 4:
            return
        if pending:
            if not pending.is_file() or pending.stat().st_size < 10000:
                return
            REPORT['images'].append(str(pending))
            pending, prepared, last = None, False, now
            shot_index += 1
            if shot_index == len(shots):
                level.set_level_viewport_camera_info(loc, rot, 'None')
                console('r.ScreenPercentage 100')
                for a in bases:
                    a.set_is_temporarily_hidden_in_editor(False)
                    a.skeletal_mesh_component.set_play_rate(1)
                    # PIE should initialize from persistent source instances, not editor MIDs.
                    a.toon_presentation.initialize(None)
                pie_started = now
                level.editor_request_begin_play()
            return
        label, selected, percent = shots[shot_index]
        if prepared:
            pending = OUT / (label+'.png')
            console(f'HighResShot 1280x720 filename="{pending.as_posix()}"')
            last = now
            return
        console('r.ScreenPercentage '+str(percent))
        for i, (a, clip) in enumerate(zip(bases, CLIPS)):
            a.set_is_temporarily_hidden_in_editor(selected is not None and selected != i)
            a.skeletal_mesh_component.set_position(clip['duration']*clip['sample_fraction'], False)
            a.toon_presentation.refresh_presentation()
        if selected is None:
            shot_loc, shot_rot = loc, rot
        else:
            target = bases[selected].get_actor_location() + unreal.Vector(0, 0, 75)
            shot_loc = target + unreal.Vector(120, 350, 130)
            shot_rot = unreal.MathLibrary.find_look_at_rotation(shot_loc, target)
        level.set_level_viewport_camera_info(shot_loc, shot_rot, 'None')
        prepared, last = True, now
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

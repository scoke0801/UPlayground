"""Editor-only, unsaved 1/10/50 actor fixture; launched by RunToonPerformance.py."""
import json
import os
import sys
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from RunToonPerformance import csv_complete
OUT = Path(os.environ['PG_TOON_PERF_OUT'])
OPTIONS = json.loads((OUT / 'options.json').read_text())
DATA = json.loads((ROOT / 'Saved/ToonTest/lighting_lab.json').read_text())
CLIPS = json.loads((ROOT / 'Saved/ToonTest/retarget.json').read_text())['clips']
ANIMATIONS = [unreal.load_asset(clip['asset']) for clip in CLIPS]
assert all(ANIMATIONS)
ROW = next(row for row in DATA['characters'] if row['name'] == 'Inori')
REPORT = {'status': 'RUNNING', 'phases': [], 'settings': {}, 'viewport_size': [], 'images': []}
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Art/ToonTest/Maps/L_PGToon_AdvancedMotionTest')
assert world
mesh = unreal.load_asset(ROW['mesh'])
candidate_mesh = None
if OPTIONS.get('lod_candidate') or OPTIONS.get('fixed_lod') is not None:
    candidate = json.loads((ROOT/'Saved/ToonTest/performance_lod.json').read_text())
    assert candidate['status'] == 'PASS'
    candidate_mesh = unreal.load_asset(candidate['mesh'])
    assert candidate_mesh
materials = [unreal.load_asset(row['asset']) for row in ROW['materials']]
overlays = [i for i, row in enumerate(ROW['materials']) if row['overlay']]
outline = unreal.load_asset(DATA['outline'])
assert mesh and outline and all(materials) and overlays
# Editor mesh inspection is prohibited during PIE in UE 5.8. Cache section
# mappings before starting play; runtime toggles use only component APIs.
mesh_editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
SECTION_MAPS = {}
for asset in [mesh] + ([candidate_mesh] if candidate_mesh else []):
    for lod in range(mesh_editor.get_lod_count(asset)):
        slots = [mesh_editor.get_lod_material_slot(asset, lod, section)
                 for section in range(mesh_editor.get_num_sections(asset, lod))]
        assert slots and all(slot >= 0 for slot in slots), (asset, lod, slots)
        SECTION_MAPS[(asset.get_path_name(), lod)] = slots
# Repeats bracket each ablation. No global shadow/translucency show flags:
# toggles affect only the fixture characters (and its outline blendable).
PHASES = [dict(name='one', count=1), dict(name='ten', count=10),
          dict(name='full_a'), dict(name='no_post', post=False), dict(name='full_b'),
          dict(name='no_outline', post=False, depth=False), dict(name='full_c'),
          dict(name='no_transparency', transparency=False), dict(name='full_d'),
          dict(name='no_shadows', shadows=False), dict(name='full_e'), dict(name='one_return', count=1)]
if OPTIONS.get('lod_candidate'):
    PHASES = [dict(name='full_a'), dict(name='lod1', lod=1), dict(name='full_b'),
              dict(name='lod1_repeat', lod=1), dict(name='full_c'), dict(name='lod2', lod=2),
              dict(name='full_d'), dict(name='lod2_repeat', lod=2), dict(name='full_e')]
    REPORT['candidate'] = candidate
if OPTIONS.get('repeats_only'):
    PHASES = [dict(name='full_a'), dict(name='full_b'), dict(name='full_c')]
if OPTIONS.get('fixed_lod') is not None:
    PHASES = [dict(phase, lod=OPTIONS['fixed_lod']) for phase in PHASES]
    REPORT['candidate'] = candidate
game = None
population = []
game_key = game_pp = camera = None
axes = {}
phase_index = 0
phase_start = capture_start = None
capturing = False
started = time.monotonic()
pie_requested = False
finished = False
stopped_at = None
busy = False
pending_shot = None
pending_finalize = None
motion_start = None
motion_verified = False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def console(command):
    unreal.SystemLibrary.execute_console_command(game or world, command)


def write_report():
    (OUT / 'runtime.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')


def finish(error=None):
    global finished, stopped_at
    if finished:
        return
    finished = True
    if capturing:
        console('CsvProfile STOP')
        if OPTIONS.get('trace'):
            console('Trace.RegionEnd PGToon_'+PHASES[phase_index]['name'])
    REPORT['status'] = 'FAIL' if error else 'PASS'
    if error:
        REPORT['error'] = error
        unreal.log_error(error)
    write_report()
    performance.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    if game:
        unreal.PGToonPreviewActor.set_preview_viewport_size(game, 0, 0)
        level.editor_request_end_play()
    # EndPlay is deferred. Let the world and render resources finish teardown
    # before requesting editor shutdown, as in the other temporal probes.
    stopped_at = time.monotonic()


def spawn(index):
    actor = unreal.PGToonPreviewActor.spawn_preview_actor(game, unreal.Transform())
    assert actor
    c = actor.skeletal_mesh_component
    c.set_skeletal_mesh_asset(mesh)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_custom_depth_stencil_value(73)
    c.set_forced_lod(1)  # fixed LOD0 for repeatable section removal
    for i, material in enumerate(materials):
        c.set_material(i, material)
    for key, value in axes.items():
        actor.toon_presentation.set_editor_property(key, value)
    actor.toon_presentation.set_editor_property('key_light', game_key)
    actor.toon_presentation.initialize(c)
    c.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    return actor


def prepare(phase):
    global motion_start, motion_verified
    motion_start, motion_verified = None, False
    count = phase.get('count', 50)
    while len(population) > count:
        population.pop().destroy_actor()
    while len(population) < count:
        population.append(spawn(len(population)))
    for i, actor in enumerate(population):
        c = actor.skeletal_mesh_component
        lod = phase.get('lod', 0)
        selected_mesh = candidate_mesh if 'lod' in phase else mesh
        if c.get_skeletal_mesh_asset() != selected_mesh:
            actor.toon_presentation.initialize(None)
            c.set_skeletal_mesh_asset(selected_mesh)
            for slot, material in enumerate(materials):
                c.set_material(slot, material)
            actor.toon_presentation.initialize(c)
        c.set_forced_lod(lod+1)
        actor.set_actor_location(unreal.Vector((i % 10 - 4.5)*110, (i // 10 - 2)*135, 0), False, False)
        c.set_render_custom_depth(phase.get('depth', True))
        c.set_cast_shadow(phase.get('shadows', True))
        # Property edits above can recreate the animation instance. Supply the
        # persistent source data as well as runtime playback after those edits.
        data = unreal.SingleAnimationPlayData()
        data.anim_to_play = ANIMATIONS[i % len(CLIPS)]
        data.saved_looping = data.saved_playing = True
        data.saved_play_rate = 1
        c.set_editor_property('animation_data', data)
        c.play_animation(data.anim_to_play, True)
        c.set_component_tick_enabled(True)
        c.set_position(CLIPS[i % len(CLIPS)]['duration']*.25, False)
        c.set_play_rate(1)
        assert c.is_playing() and c.is_component_tick_enabled()
        c.show_all_material_sections(lod)
        for section, slot in enumerate(SECTION_MAPS[(selected_mesh.get_path_name(), lod)]):
            if slot in overlays:
                c.show_material_section(slot, section, phase.get('transparency', True), lod)
        for slot in overlays:
            assert c.is_material_section_shown(slot, lod) == phase.get('transparency', True)
    game_pp.add_or_update_blendable(outline, 1. if phase.get('post', True) else 0.)
    assert len(unreal.GameplayStatics.get_all_actors_of_class(game, unreal.PGToonPreviewActor)) == count
    unreal.log('PGToonPerformance phase='+phase['name'])


def motion_sample():
    result = []
    for actor in population:
        c = actor.skeletal_mesh_component
        assert c.is_playing() and c.get_play_rate() == 1
        phase = PHASES[phase_index]
        assert all(c.is_material_section_shown(slot, phase.get('lod', 0)) == phase.get('transparency', True)
                   for slot in overlays), 'Section visibility changed after animation initialization'
        hand = c.get_socket_location('hand_l')-actor.get_actor_location()
        result.append(dict(position=c.get_position(), hand=[hand.x, hand.y, hand.z]))
    return result


def advance(now):
    global phase_index, phase_start, pending_finalize
    phase_index += 1
    if phase_index == len(PHASES):
        # STOP is asynchronous. Keep ticking until the writer releases the
        # final CSV with its footer, before EndPlay/quit can discard buffered rows.
        pending_finalize = time.monotonic()
        return
    prepare(PHASES[phase_index])
    phase_start = time.monotonic()


def tick(_dt):
    global game, game_key, game_pp, camera, axes, pie_requested, phase_start, capture_start, phase_index, capturing, pending_shot, motion_start, motion_verified, busy
    if busy:
        return
    busy = True
    try:
        now = time.monotonic()
        if finished:
            current_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not current_world and now-stopped_at > 3:
                population.clear()
                game = game_key = game_pp = camera = None
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            return
        if now-started > OPTIONS['timeout']-30:
            finish('Fixture timeout')
            return
        if pending_finalize is not None:
            if csv_complete(REPORT['phases'][-1]['csv']):
                REPORT['csv_finalized'] = True
                finish()
            elif now-pending_finalize > 30:
                finish('Final CSV did not finish writing within 30 seconds')
            return
        if pending_shot:
            if now-pending_shot['start'] >= 1 and not pending_shot['requested']:
                console(f'HighResShot {OPTIONS["width"]}x{OPTIONS["height"]} filename="{pending_shot["path"].as_posix()}"')
                pending_shot['requested'] = True
            if now-pending_shot['start'] >= 4:
                path = pending_shot['path']
                assert path.is_file() and path.stat().st_size > 10000, path
                REPORT['images'].append(str(path))
                pending_shot = None
                advance(now)
            return
        if not pie_requested:
            if now-started < 25:
                return
            # Freeze editor preview components to avoid a second animated view.
            for actor in actors.get_all_level_actors():
                if isinstance(actor, unreal.PGToonPreviewActor):
                    actor.skeletal_mesh_component.set_update_animation_in_editor(False)
            level.editor_request_begin_play()
            pie_requested = True
            return
        if game is None:
            game = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not game:
                return
            game_key = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.DirectionalLight) if a.get_actor_label() == 'PG Toon Key Light')
            game_pp = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.PostProcessVolume)[0]
            camera = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.CameraActor)[0]
            existing = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.PGToonPreviewActor)
            assert existing and not unreal.GameplayStatics.get_player_pawn(game, 0)
            axes = {key: existing[0].toon_presentation.get_editor_property(key) for key in ('head_bone', 'head_forward_axis', 'head_right_axis')}
            for actor in existing:
                actor.destroy_actor()
            # All 50 fit within the view; old fixture framing cropped the front row.
            loc = unreal.Vector(0, 1550, 1300)
            camera.set_actor_location(loc, False, False)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(0, 0, 70)), False)
            camera.camera_component.set_editor_property('field_of_view', 50)
            assert unreal.PGToonPreviewActor.set_preview_viewport_size(game, OPTIONS['width'], OPTIONS['height'])
            for command in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.DynamicRes.OperationMode 0',
                            'r.VSync 0', 't.MaxFPS 0', 'r.Streaming.FullyLoadUsedTextures 1']:
                console(command)
            REPORT['settings'] = {key: unreal.SystemLibrary.get_console_variable_float_value(key) for key in
                ['r.ScreenPercentage', 'r.DynamicRes.OperationMode', 'r.VSync', 't.MaxFPS', 'sg.ShadowQuality',
                 'sg.PostProcessQuality', 'r.Shadow.Virtual.Enable', 'r.Shadow.Virtual.ResolutionLodBiasDirectional', 'r.AntiAliasingMethod']}
            REPORT['settings']['mass.UseProcessingQueue'] = unreal.SystemLibrary.get_console_variable_float_value('mass.UseProcessingQueue')
            REPORT.update(overlays=overlays, phase_plan=PHASES)
            prepare(PHASES[0])
            phase_start = time.monotonic()
            write_report()
            return
        phase = PHASES[phase_index]
        if not motion_verified and now-phase_start >= 2:
            current = motion_sample()
            if motion_start is None:
                motion_start = (now, current)
            elif now-motion_start[0] >= .25:
                previous = motion_start[1]
                assert all(abs(a['position']-b['position']) > .001 for a, b in zip(previous, current)), 'Animation clock did not advance'
                assert any(sum(abs(x-y) for x, y in zip(a['hand'], b['hand'])) > .001 for a, b in zip(previous, current)), 'No skeletal bone motion'
                motion_verified = True
                REPORT.setdefault('motion_checks', []).append(dict(phase=phase['name'], actors=len(current), bone_motion=True))
        if not capturing and now-phase_start >= OPTIONS['warmup']:
            # A render-state change can stall the first warmup tick past the
            # deadline. Still require two animation samples on distinct ticks;
            # elapsed wall time alone does not mean the fixture is ready.
            if not motion_verified:
                assert now-phase_start < OPTIONS['warmup']+30, 'Animation verification timed out before capture'
                return
            REPORT['viewport_size'] = list(unreal.GameplayStatics.get_player_controller(game, 0).get_viewport_size())
            assert REPORT['viewport_size'] == [OPTIONS['width'], OPTIONS['height']]
            filename = 'toon_'+OUT.name+'_'+phase['name']+'.csv'
            if OPTIONS.get('trace'):
                console('Trace.RegionBegin PGToon_'+phase['name'])
            console('CsvProfile STARTFILE='+filename)
            console('CsvProfile START')
            capturing, capture_start = True, now
        if capturing and now-capture_start >= OPTIONS['sample']:
            console('CsvProfile STOP')
            if OPTIONS.get('trace'):
                console('Trace.RegionEnd PGToon_'+phase['name'])
            capturing = False
            REPORT['phases'].append({**phase, 'csv': str(ROOT/'Saved/Profiling/CSV'/('toon_'+OUT.name+'_'+phase['name']+'.csv')),
                'sample_seconds': now-capture_start, 'capture_start': capture_start, 'capture_end': now})
            write_report()
            if OPTIONS.get('lod_candidate') and phase['name'] in ['full_a', 'lod1', 'lod2']:
                for i, actor in enumerate(population):
                    actor.skeletal_mesh_component.set_play_rate(0)
                    actor.skeletal_mesh_component.set_position(CLIPS[i % len(CLIPS)]['duration']*.25, False)
                pending_shot = dict(start=now, path=OUT/(phase['name']+'.png'), requested=False)
            else:
                advance(now)
    except Exception:
        finish(traceback.format_exc())
    finally:
        busy = False


write_report()
handle = unreal.register_slate_post_tick_callback(tick)

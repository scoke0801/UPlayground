"""Unsaved native Shot sequence; launched only by RunToonLODQuality.py."""
import json
import math
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = Path(os.environ['PG_TOON_QUALITY_OUT'])
OPTIONS = json.loads((OUT/'options.json').read_text())
DATA = json.loads((ROOT/'Saved/ToonTest/lighting_lab.json').read_text())
ROW = next(r for r in DATA['characters'] if r['name'] == 'Inori')
clips = json.loads((ROOT/'Saved/ToonTest/retarget.json').read_text())['clips']
clips = {c['label']: c for c in clips}
clips['Dodge'] = json.loads((OUT/'prepare.json').read_text())['dodge']
PLAN = []
for mode in ['comparison', 'close']:
    for motion in ['Idle', 'Run', 'Attack', 'Dodge', 'Hit', 'Death']:
        PLAN.append(dict(name=mode+'_'+motion, mode=mode, motion=motion, frames=120))
for motion in ['Run', 'Attack']:
    PLAN.append(dict(name='sweep_'+motion, mode='sweep', motion=motion, frames=480))
if OPTIONS['smoke']:
    PLAN = [dict(name='smoke', mode='comparison', motion='Attack', frames=45)]
if OPTIONS.get('diagnostic'):
    PLAN = [dict(name=name, mode='close', motion='Run', frames=60)
            for name in ['shadow_baseline', 'shadow_single', 'shadow_no_occluder']]
if OPTIONS.get('shadow_check'):
    PLAN = [dict(name=name, mode='close', motion='Run', frames=60)
            for name in ['shadow_baseline', 'shadow_off', 'shadow_uncached', 'shadow_lod0']]
if OPTIONS.get('invalidation_check'):
    PLAN = [dict(name=name, mode='close', motion='Run', frames=90)
            for name in ['invalidate_auto', 'invalidate_always', 'invalidate_bounds']]

report = dict(status='RUNNING', options=OPTIONS, plan=PLAN, phases=[], frames=[])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Art/ToonTest/Maps/L_PGToon_AdvancedMotionTest')
mesh = unreal.load_asset('/Game/Art/ToonTest/Advanced/Performance/SK_Inori_ToonLOD')
assert world and mesh
level.editor_set_viewport_realtime(False)
materials = [unreal.load_asset(r['asset']) for r in ROW['materials']]
animations = {k: unreal.load_asset(v['asset']) for k, v in clips.items()}
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PGToonPreviewActor):
        a.skeletal_mesh_component.set_update_animation_in_editor(False)
game = camera = None
population = []
started = time.monotonic()
requested = finished = busy = False
phase_index = -1
phase_tick = 0
pending = None
warmup = 60

def console(command):
    unreal.SystemLibrary.execute_console_command(game or world, command)

def write():
    (OUT/'capture.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

def finish(error=None):
    global finished
    if finished:
        return
    finished = True
    report.update(status='FAIL' if error else 'CAPTURED', visual_review_required=True)
    if error:
        report['error'] = error
        unreal.log_error(error)
    write()
    performance.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    unreal.unregister_slate_post_tick_callback(handle)
    if game:
        unreal.PGToonPreviewActor.set_preview_viewport_size(game, 0, 0)
        level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def view(distance):
    aim = unreal.Vector(0, 0, 78)
    offset = unreal.Vector(0, .819152*distance, .573576*distance)
    location = aim+offset
    camera.set_actor_location(location, False, False)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(location, aim), False)

def prepare():
    global phase_index, phase_tick
    phase_index += 1
    phase_tick = -warmup
    if phase_index == len(PLAN):
        finish()
        return
    phase = PLAN[phase_index]
    (OUT/phase['name']).mkdir()
    if not OPTIONS.get('diagnostic') and not OPTIONS.get('shadow_check'):
        # Diagnostic setup flush only. Artifacts may recur with caching enabled;
        # this is not treated as a production shadow fix.
        console('r.Shadow.Virtual.Cache 0')
    for i, a in enumerate(population):
        c = a.skeletal_mesh_component
        show = phase['mode'] == 'comparison' or i == 1
        a.set_actor_hidden_in_game(not show)
        a.set_actor_location(unreal.Vector((1-i)*215 if phase['mode'] == 'comparison' else 0, 0, 0), False, False)
        c.set_forced_lod(i+1 if phase['mode'] == 'comparison' else 0 if phase['mode'] == 'sweep' else 2)
        c.play_animation(animations[phase['motion']], True)
        c.set_position(0, False)
        c.set_play_rate(1)
    if phase['name'] in ['shadow_single', 'shadow_no_occluder']:
        for i,a in enumerate(population):
            if i != 1:
                a.skeletal_mesh_component.set_cast_shadow(False)
                a.skeletal_mesh_component.set_render_custom_depth(False)
    if phase['name'] == 'shadow_no_occluder':
        for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.StaticMeshActor):
            if a.get_actor_label() == 'PG Toon Shadow Occluder':
                a.destroy_actor()
    if OPTIONS.get('shadow_check'):
        for i,a in enumerate(population):
            a.skeletal_mesh_component.set_cast_shadow(i == 1 and phase['name'] != 'shadow_off')
        console('r.Shadow.Virtual.Cache '+('0' if phase['name'] in ['shadow_uncached', 'shadow_lod0'] else '1'))
        if phase['name'] == 'shadow_lod0':
            population[1].skeletal_mesh_component.set_forced_lod(1)
    if OPTIONS.get('invalidation_check'):
        for a in population:
            a.skeletal_mesh_component.set_editor_property('shadow_cache_invalidation_behavior',
                unreal.ShadowCacheInvalidationBehavior.AUTO if phase['name'] == 'invalidate_auto'
                else unreal.ShadowCacheInvalidationBehavior.ALWAYS)
            a.skeletal_mesh_component.set_bounds_scale(1.5 if phase['name'] == 'invalidate_bounds' else 1.)
    # Editor property changes can recreate the animation instance. Persist and
    # start playback after ALL rendering-property mutations, never before them.
    for a in population:
        c = a.skeletal_mesh_component
        data = unreal.SingleAnimationPlayData()
        data.anim_to_play = animations[phase['motion']]
        data.saved_playing = data.saved_looping = True
        data.saved_play_rate = 1.
        c.set_editor_property('animation_data', data)
        c.play_animation(data.anim_to_play, True)
        c.set_position(0, False)
        c.set_play_rate(1)
    view(850 if phase['mode'] == 'comparison' else 460 if phase['mode'] == 'close' else 300)
    report['phases'].append(phase | {'start_index': len(report['frames']),
        'shadow_invalidation': str(population[1].skeletal_mesh_component.get_editor_property('shadow_cache_invalidation_behavior')),
        'bounds_scale': population[1].skeletal_mesh_component.get_editor_property('bounds_scale')})
    write()

def sample():
    result = []
    for a in population:
        c = a.skeletal_mesh_component
        assert c.is_playing() and c.get_play_rate() == 1, 'Animation stopped after fixture configuration'
        head = c.get_socket_location('head_x') - a.get_actor_location()
        hand = c.get_socket_location('hand_l') - a.get_actor_location()
        result.append(dict(lod=c.get_predicted_lod_level(), position=c.get_position(),
                           head=[head.x, head.y, head.z], hand=[hand.x, hand.y, hand.z]))
    return result

def tick(dt):
    global busy, game, camera, requested, pending, phase_tick
    if busy or finished:
        return
    busy = True
    try:
        if time.monotonic()-started > 1700:
            raise RuntimeError('Capture timed out')
        if not requested:
            if time.monotonic()-started < 15:
                return
            level.editor_request_begin_play()
            requested = True
            return
        if game is None:
            game = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not game:
                return
            assert not unreal.GameplayStatics.get_player_pawn(game, 0)
            existing = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.PGToonPreviewActor)
            report['fixture_actors'] = [{'label': a.get_actor_label(), 'class': a.get_class().get_name(),
                                        'location': str(a.get_actor_location())}
                                       for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Actor)]
            axes = {k: existing[0].toon_presentation.get_editor_property(k) for k in ['head_bone', 'head_forward_axis', 'head_right_axis']}
            for a in existing:
                a.destroy_actor()
            for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.TextRenderActor):
                a.set_actor_hidden_in_game(True)
            key = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.DirectionalLight)
                       if a.get_actor_label() == 'PG Toon Key Light')
            camera = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.CameraActor)[0]
            camera.camera_component.set_editor_property('field_of_view', 50)
            for i in range(3):
                a = unreal.PGToonPreviewActor.spawn_preview_actor(game, unreal.Transform())
                c = a.skeletal_mesh_component
                c.set_skeletal_mesh_asset(mesh)
                c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                c.set_render_custom_depth(True)
                c.set_custom_depth_stencil_value(73)
                for slot, material in enumerate(materials):
                    c.set_material(slot, material)
                for k,v in axes.items():
                    a.toon_presentation.set_editor_property(k,v)
                a.toon_presentation.set_editor_property('key_light', key)
                a.toon_presentation.initialize(c)
                c.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
                population.append(a)
            assert unreal.PGToonPreviewActor.set_preview_viewport_size(game, OPTIONS['width'], OPTIONS['height'])
            for cmd in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.DynamicRes.OperationMode 0',
                        'r.VSync 0', 't.MaxFPS 0', 'r.Streaming.FullyLoadUsedTextures 1']:
                console(cmd)
            report['settings'] = {k: unreal.SystemLibrary.get_console_variable_float_value(k) for k in
                ['r.ScreenPercentage', 'r.AntiAliasingMethod', 'r.Shadow.Virtual.Enable', 'r.SkeletalMeshLODBias',
                 'r.ViewDistanceScale', 'r.SkeletalMeshLODRadiusScale', 'sg.ShadowQuality']}
            report['viewport'] = list(unreal.GameplayStatics.get_player_controller(game, 0).get_viewport_size())
            assert report['viewport'] == [OPTIONS['width'], OPTIONS['height']]
            prepare()
            return
        # Shot was requested after the preceding rendered frame. The next tick
        # confirms it completed; never silently skip, duplicate or wait out frames.
        if pending:
            path, row = pending
            assert path.exists() and path.stat().st_size > 1000, 'Missing frame '+str(path)
            # Read PNG IHDR without external packages inside the editor.
            import struct
            assert struct.unpack('>II', path.read_bytes()[16:24]) == (OPTIONS['width'], OPTIONS['height']), 'Wrong viewport captured: '+str(path)
            row['rendered_world_time'] = unreal.GameplayStatics.get_time_seconds(game)
            row['rendered_pose'] = sample()
            report['frames'].append(row)
            pending = None
        phase = PLAN[phase_index]
        if phase_tick >= phase['frames']:
            prepare()
            return
        if phase_tick < 0:
            if phase_tick == -warmup+5 and not OPTIONS.get('diagnostic') and not OPTIONS.get('shadow_check'):
                console('r.Shadow.Virtual.Cache 1')
            # Prime screenshot ownership while PIE takes over the editor pane.
            # Startup draws can otherwise consume the global screenshot request.
            if phase_index == 0 and -10 <= phase_tick <= -5:
                console(f'Shot filename="{(OUT / ("warmup_"+str(-phase_tick)+".png")).as_posix()}" -nosuffix')
            phase_tick += 1
            if phase_tick == 0:
                for a in population:
                    a.skeletal_mesh_component.set_position(0, False)
            return
        if phase['mode'] == 'sweep':
            f = phase_tick/(phase['frames']-1)
            # Smoothly travel out and back, covering both hysteresis directions.
            distance = 300 + 2500*(.5-.5*math.cos(f*2*math.pi))
            view(distance)
        else:
            distance = 850 if phase['mode'] == 'comparison' else 460
        path = OUT/phase['name']/f'{phase_tick:05d}.png'
        row = dict(phase=phase['name'], frame=phase_tick, distance_cm=distance,
                   request_world_time=unreal.GameplayStatics.get_time_seconds(game), pose=sample(),
                   shadow_cache=unreal.SystemLibrary.get_console_variable_float_value('r.Shadow.Virtual.Cache'))
        console(f'Shot filename="{path.as_posix()}" -nosuffix')
        pending = (path,row)
        phase_tick += 1
    except Exception:
        finish(traceback.format_exc())
    finally:
        busy = False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
write()
handle = unreal.register_slate_post_tick_callback(tick)

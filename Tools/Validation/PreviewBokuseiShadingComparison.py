"""Reload cumulative stages, render comparisons and verify the playable input path."""
import hashlib
import json
import math
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT/'Saved/BokuseiShadingComparison'
RUN = OUT/'Preview'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
RUN.mkdir(parents=True)
DATA = json.loads((OUT/'configure.json').read_text(encoding='utf-8'))
REPORT = dict(status='RUNNING', run=str(RUN), map=DATA['map'], images=[])
LIB = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.EditorLoadingAndSavingUtils.load_map(DATA['map'])
assert world and DATA['status'] == 'PASS'
all_actors = actors.get_all_level_actors()
models = sorted([a for a in all_actors if isinstance(a, unreal.SkeletalMeshActor)], key=lambda a: a.get_actor_location().x)
STAGES = len(DATA['stages'])
SDF_PROBE = '-PGFaceSDFProbe' in unreal.SystemLibrary.get_command_line()
assert DATA['schema'] in [4,5] and len(models) == STAGES and STAGES in [6,8]
if SDF_PROBE:
    sdf_data = json.loads((ROOT/'Saved/BokuseiFaceSDF/configure.json').read_text(encoding='utf-8'))
    assert sdf_data['status'] == 'PASS'
    if sdf_data['applied']:
        applied_face = unreal.load_asset(sdf_data['source_face'])
        candidate = unreal.load_asset(sdf_data['sdf_face'])
        assert applied_face.get_editor_property('parent') == candidate.get_editor_property('parent')
        for name,value in sdf_data['parameters'].items():
            assert abs(LIB.get_material_instance_scalar_parameter_value(applied_face,name)-value) < 1e-6,name
        assert LIB.get_material_instance_texture_parameter_value(applied_face,'FaceSDFTexture') == unreal.load_asset(sdf_data['texture'])
        REPORT['applied_face_reload'] = dict(status='PASS',material=applied_face.get_path_name())
components = [a.skeletal_mesh_component for a in models]
left, right = components[0], components[4]
assert left.get_skeletal_mesh_asset() == right.get_skeletal_mesh_asset()
assert left.get_editor_property('forced_lod_model') == right.get_editor_property('forced_lod_model') == 1
assert right.get_editor_property('leader_pose_component') == left
assert not left.get_editor_property('render_custom_depth')
assert right.get_editor_property('render_custom_depth') and right.get_editor_property('custom_depth_stencil_value') == 73
assert isinstance(models[1], unreal.PGToonPreviewActor)
assert models[1].toon_presentation.key_light
cloth = json.loads((ROOT/'Tools/Art/ToonTest/shading_profiles.json').read_text(encoding='utf-8'))['profiles']['cloth']
for index, (actor, component, stage) in enumerate(zip(models, components, DATA['stages'])):
    assert actor.actor_has_tag('PGShadingStage'+str(index))
    assert component.get_skeletal_mesh_asset() == left.get_skeletal_mesh_asset()
    assert component.get_editor_property('forced_lod_model') == 1
    assert component.get_editor_property('render_custom_depth') == (index >= 4)
    if index:
        assert component.get_editor_property('leader_pose_component') == left
        assert isinstance(actor, unreal.PGToonPreviewActor) and actor.toon_presentation.key_light
    for slot_index, row in enumerate(DATA['slots']):
        material = component.get_material(slot_index)
        source = right.get_material(slot_index)
        assert material.get_path_name() == stage['materials'][slot_index]
        for name in ['BaseTexture', 'OpacityTexture']:
            assert LIB.get_material_instance_texture_parameter_value(material, name) == LIB.get_material_instance_texture_parameter_value(source, name)
        for name in ['UseBaseAlpha', 'AlphaMaskMode', 'AlphaMaskScale', 'AlphaMaskValue', 'MainOpacity', 'OpacityCutoff']:
            assert abs(LIB.get_material_instance_scalar_parameter_value(material, name)-LIB.get_material_instance_scalar_parameter_value(source, name)) < 1e-6
        ca, cb = [LIB.get_material_instance_vector_parameter_value(m, 'BaseTint') for m in [material, source]]
        assert all(abs(getattr(ca, axis)-getattr(cb, axis)) < 1e-6 for axis in ['r', 'g', 'b', 'a'])
        if index in [1, 2]:
            assert material.get_editor_property('parent') == source
            assert LIB.get_material_instance_scalar_parameter_value(material, 'RimStrength') == 0
            assert LIB.get_material_instance_scalar_parameter_value(material, 'SpecularStrength') == 0
        if index == 1:
            for name in ['DiffuseWrap', 'ShadowSoftness', 'LightSoftness', 'BandAA', 'ShadeStrength']:
                assert abs(LIB.get_material_instance_scalar_parameter_value(material, name)-cloth['scalars'][name]) < 1e-6
        if index == 3:
            assert material == source
        if index == 5:
            parent = material.get_editor_property('parent')
            assert parent.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_DEFAULT_LIT
            assert parent.get_editor_property('blend_mode') == source.get_editor_property('parent').get_editor_property('blend_mode')
            assert abs(LIB.get_material_instance_scalar_parameter_value(material, 'WorldLightingInfluence')-.65) < 1e-6
            assert LIB.get_material_instance_scalar_parameter_value(material, 'FaceShading') == 0
            assert LIB.get_material_instance_scalar_parameter_value(material, 'HairAnisotropy') == 0
            for name in LIB.get_scalar_parameter_names(source):
                assert abs(LIB.get_material_instance_scalar_parameter_value(material, name)-LIB.get_material_instance_scalar_parameter_value(source, name)) < 1e-6, name
        if index in [6,7] and row['name'] == 'Mat_Bokusei_Face':
            assert LIB.get_material_instance_scalar_parameter_value(material,'FaceSDFEnabled') == 1
            texture = LIB.get_material_instance_texture_parameter_value(material,'FaceSDFTexture')
            assert texture and not texture.get_editor_property('srgb')
            assert texture.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP
            assert material.get_editor_property('parent').get_editor_property('shading_model') == (unreal.MaterialShadingModel.MSM_UNLIT if index == 6 else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        elif index == 6:
            assert material == source
        elif index == 7:
            assert material == components[5].get_material(slot_index)
shadow_casters = [a for a in all_actors if a.actor_has_tag('PGShadingShadowCaster')]
assert len(shadow_casters) == STAGES-4
for caster in shadow_casters:
    c = caster.static_mesh_component
    assert not c.get_editor_property('cast_shadow') and c.get_editor_property('cast_hidden_shadow')
    assert not c.get_editor_property('visible')
hair_proxy = models[5].hair_shadow_proxy
hair_proxies = [a.hair_shadow_proxy for a in models[1:] if a.hair_shadow_proxy.get_skeletal_mesh_asset()]
assert len(hair_proxies) == (2 if STAGES == 8 else 1)
for proxy in hair_proxies:
    assert proxy.get_skeletal_mesh_asset() == left.get_skeletal_mesh_asset()
    assert proxy.get_editor_property('leader_pose_component') == left
    assert proxy.get_editor_property('forced_lod_model') == 1
    assert proxy.get_editor_property('cast_shadow') and proxy.get_editor_property('cast_hidden_shadow')
    for name in ['visible', 'render_in_main_pass', 'render_in_depth_pass', 'render_custom_depth']:
        assert not proxy.get_editor_property(name), name
    for i, path in enumerate(DATA['hair_shadow']['materials']):
        assert proxy.get_material(i).get_path_name() == path
for actor in models[1:5]:
    assert not actor.hair_shadow_proxy.get_skeletal_mesh_asset()
for row in DATA['hair_shadow']['slots']:
    mi = hair_proxy.get_material(row['index'])
    source = right.get_material(row['index'])
    assert mi.get_editor_property('parent').get_editor_property('blend_mode') == unreal.BlendMode.BLEND_MASKED
    for name in ['BaseTexture', 'OpacityTexture']:
        assert LIB.get_material_instance_texture_parameter_value(mi, name) == LIB.get_material_instance_texture_parameter_value(source, name)
    for name, value in row['alpha'].items():
        assert abs(LIB.get_material_instance_scalar_parameter_value(mi, name)-value) < 1e-6
    assert abs(LIB.get_material_instance_scalar_parameter_value(mi, 'ShadowInset')-DATA['hair_shadow']['shadow_inset_cm']) < 1e-6
for index, row in enumerate(DATA['slots']):
    a, b = left.get_material(index), right.get_material(index)
    assert isinstance(a, unreal.MaterialInstanceConstant) and isinstance(b, unreal.MaterialInstanceConstant)
    assert a.get_path_name() == row['lit'] and b.get_path_name() == row['toon']
    assert a.get_editor_property('parent').get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    assert a.get_editor_property('parent').get_editor_property('blend_mode') == b.get_editor_property('parent').get_editor_property('blend_mode')
    for name in ['BaseTexture', 'OpacityTexture']:
        assert LIB.get_material_instance_texture_parameter_value(a, name) == LIB.get_material_instance_texture_parameter_value(b, name)
    for name in ['UseBaseAlpha', 'AlphaMaskMode', 'AlphaMaskScale', 'AlphaMaskValue', 'MainOpacity', 'OpacityCutoff']:
        assert abs(LIB.get_material_instance_scalar_parameter_value(a, name)-LIB.get_material_instance_scalar_parameter_value(b, name)) < 1e-6
    ca = LIB.get_material_instance_vector_parameter_value(a, 'BaseTint')
    cb = LIB.get_material_instance_vector_parameter_value(b, 'BaseTint')
    assert all(abs(getattr(ca, axis)-getattr(cb, axis)) < 1e-6 for axis in ['r', 'g', 'b', 'a'])
for relative, expected in DATA['protected'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest() == expected, relative
mode = world.get_world_settings().get_editor_property('default_game_mode')
assert unreal.get_default_object(mode).get_editor_property('default_pawn_class') == unreal.PGShadingComparisonPawn.static_class()
cameras = [a for a in all_actors if isinstance(a, unreal.CameraActor)]
assert len(cameras) == 3
shots = [(name, next(c for c in cameras if fragment in c.get_actor_label()))
         for name, fragment in [('Warmup', '정면'), ('Front', '정면'), ('Quarter', '쿼터뷰'), ('Face', '얼굴')]]
# Fixed face views make the cel -> per-part softness -> highlights delta inspectable.
shots += [('Stage'+str(i+1)+'Face', None, unreal.Vector(stage['position'], 135, 144),
           unreal.MathLibrary.find_look_at_rotation(unreal.Vector(stage['position'], 135, 144), unreal.Vector(stage['position'], 0, 139)))
          for i, stage in enumerate(DATA['stages'])]
shots += [('Stage'+str(i+1)+'Shadow'+('On' if enabled else 'Off'), None, unreal.Vector(DATA['stages'][i]['position'], 135, 144),
           unreal.MathLibrary.find_look_at_rotation(unreal.Vector(DATA['stages'][i]['position'], 135, 144),
                                                    unreal.Vector(DATA['stages'][i]['position'], 0, 139)), enabled)
          for i in [4, 5] for enabled in [True, False]]
base = DATA['stages'][5]['position']
shots += [('Stage6HairShadowOff', None, unreal.Vector(base, 135, 144),
           unreal.MathLibrary.find_look_at_rotation(unreal.Vector(base, 135, 144), unreal.Vector(base, 0, 139)), False, False)]
shots += [('Stage6HairQuarter'+('On' if enabled else 'Off'), None, unreal.Vector(base+65, 115, 150),
           unreal.MathLibrary.find_look_at_rotation(unreal.Vector(base+65, 115, 150), unreal.Vector(base, 0, 139)), False, enabled)
          for enabled in [True, False]]
key_light = models[1].toon_presentation.key_light
key_rotation = key_light.get_actor_rotation()
if SDF_PROBE:
    assert STAGES == 8
    # Isolate the face effect with identical source/SDF poses and world-light angles.
    shots = shots[:4]+[s for s in shots if s[0] in ['Stage5Face','Stage6Face','Stage7Face','Stage8Face']]
    for stage_index in [4,6]:
        for angle in [-60,0,60,180]:
            theta = math.radians(angle)
            L = unreal.Vector(math.sin(theta)*.94,math.cos(theta)*.94,.342)
            rotation = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(),L*-1)
            base = DATA['stages'][stage_index]['position']
            location = unreal.Vector(base,135,144)
            aim = unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(base,0,139))
            shots.append((f'Stage{stage_index+1}Light{angle:+04d}',None,location,aim,False,False,rotation))
    for name,L in [('Zenith',unreal.Vector(0,0,1)),('Nadir',unreal.Vector(0,0,-1))]:
        base = DATA['stages'][6]['position']
        location = unreal.Vector(base,135,144)
        shots.append(('SDF'+name,None,location,unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(base,0,139)),False,False,
                      unreal.MathLibrary.find_look_at_rotation(unreal.Vector(),L*-1)))
    for stage_index in [4,6]:
        base = DATA['stages'][stage_index]['position']
        location = unreal.Vector(base+230,390,330)
        shots.append((f'Stage{stage_index+1}Quarter',None,location,unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(base,0,90)),False,True,key_rotation))
    REPORT['face_sdf'] = dict(status='RUNNING',paired_angles=[-60,0,60,180],zenith_nadir=True,source_stage=5,sdf_stage=7,world_stage=8)
if '-PGComparisonInputOnly' in unreal.SystemLibrary.get_command_line():
    shots = []
    REPORT['editor_render_skipped'] = True
    if SDF_PROBE: REPORT['face_sdf']['status'] = 'SKIPPED'
REPORT['reload_validation'] = dict(status='PASS', matched_slots=len(DATA['slots']), original_assets_unchanged=True,
                                    synchronized_pose=True, outline_stages=list(range(5,STAGES+1)), stages=STAGES, shadow_casters=STAGES-4,
                                    hair_shadow_slots=2, hair_proxy_hidden=True)
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
previous_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)
idle = unreal.load_asset(DATA['animation'])
left.override_animation_data(idle, True, False, 1.25, 0)
for actor, component in zip(models[1:], components[1:]):
    actor.toon_presentation.initialize(component)
for command in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.Streaming.FullyLoadUsedTextures 1', 'viewmode lit', 't.MaxFPS 60']:
    unreal.SystemLibrary.execute_console_command(world, command)
level.editor_set_game_view(True)
started = last = time.monotonic()
index, pending, prepared, pie_started, first = 0, None, False, None, None
finished = False
probe = None
probe_at = 0
probe_start = None
probe_rotation = None
probe_index = 0
input_cases = ([('stage', key, i) for i, key in enumerate(['One', 'Two', 'Three', 'Four', 'Five', 'Six','Seven','Eight'][:STAGES])]
               + [('stage','Six',5),('face', 'F', 5), ('shadow_on', 'H', None), ('shadow_off', 'H', None),
                  ('hair_off', 'J', None), ('hair_on', 'J', None),
                  ('face_stage', 'Three', 2), ('quarter', 'C', 2), ('reset', 'R', None)]
               + [('move', key, None) for key in ['W', 'S', 'A', 'D', 'Q', 'E']]
               + [('fast', 'W', None), ('look_idle', 'MouseX', None), ('look', 'MouseX', None),
                  ('look_idle', 'MouseY', None), ('look', 'MouseY', None), ('reset', 'Zero', None)])
REPORT['input_checks'] = []
if STAGES == 8:
    input_cases += [('stage','Seven',6),('face','F',6),('face_stage','Eight',7),('quarter','C',7)]
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def vec(v):
    return [v.x, v.y, v.z]


def key(pawn, name, pressed, axis=0):
    # Return value is whether the engine consumed the key; polling movement keys
    # intentionally has no action binding. Validate the observed result instead.
    input_key = unreal.Key()
    input_key.set_editor_property('key_name', name)
    pawn.send_probe_input(input_key, pressed, axis)


def test_inputs(game_world, now):
    global probe, probe_at, probe_start, probe_rotation, probe_index
    pc = unreal.GameplayStatics.get_player_controller(game_world, 0)
    pawn = pc.get_controlled_pawn()
    assert isinstance(pawn, unreal.PGShadingComparisonPawn) and pc.get_view_target() == pawn
    if probe_index == len(input_cases):
        pawn.show_overview()
        assert unreal.PGEditorProbeTools.capture_game_viewport(game_world, str(RUN/'PlayableOverview.png'))
        REPORT['images'].append(str(RUN/'PlayableOverview.png'))
        REPORT['free_camera'] = dict(status='PASS', possessed=True, input_cases=len(input_cases), pawns=1)
        finish()
        return
    kind, name, stage = input_cases[probe_index]
    if probe is None:
        if kind in ['move', 'fast', 'look_idle', 'look']:
            pawn.show_overview()
        probe_start = pawn.get_actor_location()
        probe_rotation = pc.get_control_rotation()
        if kind == 'fast': key(pawn, 'LeftShift', True)
        if kind == 'look': key(pawn, 'RightMouseButton', True)
        if kind == 'shadow_on':
            runtime_models = unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.SkeletalMeshActor)
            leader = next(a.skeletal_mesh_component for a in runtime_models if a.actor_has_tag('PGShadingStage0'))
            leader.override_animation_data(idle, True, False, 1.25, 0)
        key(pawn, name, True, 10 if name in ['MouseX', 'MouseY'] else 0)
        if kind not in ['move', 'fast', 'look', 'look_idle']: key(pawn, name, False)
        probe, probe_at = kind, now
        return
    if now-probe_at < (.8 if kind in ['move', 'fast'] else .4):
        # Mouse deltas are sampled each frame. A single event immediately followed
        # by zero is filtered by the engine's mouse smoothing and isn't a drag.
        if kind in ['look', 'look_idle']: key(pawn, name, True, 10)
        return
    location = pawn.get_actor_location()
    if kind in ['move', 'fast']:
        key(pawn, name, False)
        key(pawn, 'LeftShift', False)
        direction = unreal.MathLibrary.get_forward_vector(pc.get_control_rotation()) if name in ['W', 'S'] else unreal.MathLibrary.get_right_vector(pc.get_control_rotation()) if name in ['A', 'D'] else unreal.Vector(0, 0, 1)
        sign = -1 if name in ['S', 'A', 'Q'] else 1
        delta = location-probe_start
        projection = sum(a*b for a, b in zip(vec(delta), vec(direction))) * sign
        assert projection > 15, (kind, name, projection)
        speed = pawn.get_movement_component().get_editor_property('max_speed')
        expected_speed = pawn.move_speed * (pawn.fast_multiplier if kind == 'fast' else 1)
        assert abs(speed-expected_speed) < .01
        details = dict(distance_cm=projection, max_speed=speed)
    elif kind in ['look', 'look_idle']:
        key(pawn, name, False, 0)
        rotation = pc.get_control_rotation()
        angle = abs(rotation.yaw-probe_rotation.yaw) if name == 'MouseX' else abs(rotation.pitch-probe_rotation.pitch)
        assert angle > 1 if kind == 'look' else angle < .001, (kind, angle)
        key(pawn, 'RightMouseButton', False)
        details = dict(angle_change=angle)
    elif kind == 'reset':
        expected = next(c for c in unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.CameraActor) if c.actor_has_tag('PGShadingOverview'))
        assert math.dist(vec(location), vec(expected.get_actor_location())) < .01
        details = dict(overview_restored=True)
    elif kind in ['shadow_off', 'shadow_on']:
        enabled = kind == 'shadow_on'
        casters = [a for a in unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.StaticMeshActor)
                   if a.actor_has_tag('PGShadingShadowCaster')]
        assert len(casters) == STAGES-4 and all(a.static_mesh_component.get_editor_property('cast_shadow') == enabled for a in casters)
        path = RUN/('PlayableShadowOn.png' if enabled else 'PlayableShadowOff.png')
        assert unreal.PGEditorProbeTools.capture_game_viewport(game_world, str(path))
        REPORT['images'].append(str(path))
        details = dict(casters=len(casters), enabled=enabled)
    elif kind in ['hair_off', 'hair_on']:
        enabled = kind == 'hair_on'
        hair_models = [a for a in unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.PGToonPreviewActor)
                       if a.hair_shadow_proxy.get_skeletal_mesh_asset()]
        assert len(hair_models) == (2 if STAGES == 8 else 1)
        assert all(a.hair_shadow_proxy.get_editor_property('cast_shadow') == enabled for a in hair_models)
        assert not any(a.static_mesh_component.get_editor_property('cast_shadow')
                       for a in unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.StaticMeshActor)
                       if a.actor_has_tag('PGShadingShadowCaster'))
        path = RUN/('PlayableHairShadowOn.png' if enabled else 'PlayableHairShadowOff.png')
        assert unreal.PGEditorProbeTools.capture_game_viewport(game_world, str(path))
        REPORT['images'].append(str(path))
        details = dict(enabled=enabled, hair_slots=2, external_casters_disabled=True)
        if enabled:
            leader = next(a.skeletal_mesh_component for a in unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.SkeletalMeshActor)
                          if a.actor_has_tag('PGShadingStage0'))
            leader.override_animation_data(idle, True, True, 1.25, 1)
    else:
        expected_y = 135 if kind in ['face', 'face_stage'] else 390 if kind == 'quarter' else 360
        assert abs(location.x-DATA['stages'][stage]['position']-(230 if kind == 'quarter' else 0)) < .01
        assert abs(location.y-expected_y) < .01, (kind, vec(location))
        details = dict(stage=stage+1, position=vec(location))
        if kind in ['face', 'quarter']:
            path = RUN/('Playable'+kind.title()+'.png')
            assert unreal.PGEditorProbeTools.capture_game_viewport(game_world, str(path))
            REPORT['images'].append(str(path))
    REPORT['input_checks'].append(dict(kind=kind, key=name, status='PASS', **details))
    probe, probe_index = None, probe_index+1


def finish(error=None):
    global finished
    if finished:
        return
    finished = True
    REPORT.update(status='FAIL' if error else 'PASS')
    if error:
        REPORT['error'] = error
        unreal.log_error(error)
    performance.set_editor_property('bThrottleCPUWhenNotForeground', previous_throttle)
    payload = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (OUT/'preview.json').write_text(payload, encoding='utf-8')
    (RUN/'preview.json').write_text(payload, encoding='utf-8')
    if SDF_PROBE:
        (ROOT/'Saved/BokuseiFaceSDF/preview.json').write_text(payload,encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    if pie_started:
        level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global index, pending, prepared, last, pie_started, first
    try:
        now = time.monotonic()
        if now-started > 240:
            finish('Comparison render/PIE timeout')
            return
        if pie_started:
            game_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not game_world or now-pie_started < 1:
                return
            game_models = sorted(unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.SkeletalMeshActor), key=lambda a: a.get_actor_location().x)
            assert len(game_models) == STAGES
            a = game_models[0].skeletal_mesh_component
            assert len(unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.Pawn)) == 1
            pc = unreal.GameplayStatics.get_player_controller(game_world, 0)
            pawn = pc.get_controlled_pawn()
            assert isinstance(pawn, unreal.PGShadingComparisonPawn) and pc.get_view_target() == pawn
            assert len(unreal.WidgetLibrary.get_all_widgets_of_class(game_world, unreal.PGUIShadingComparison, False)) == 1
            sample = {name: vec(a.get_socket_location(name)-game_models[0].get_actor_location())
                      for name in ['Head', 'Hand_L', 'Hand_R', 'Foot_L', 'Foot_R']}
            errors = []
            for actor in game_models[1:]:
                b = actor.skeletal_mesh_component
                assert b.get_editor_property('leader_pose_component') == a
                assert actor.toon_presentation.get_dynamic_material_count() == len(DATA['slots'])
                errors.extend(math.dist(sample[name], vec(b.get_socket_location(name)-actor.get_actor_location())) for name in sample)
            assert max(errors) < .001, ('A/B pose drift', errors)
            if STAGES == 8:
                face_index = next(i for i,row in enumerate(DATA['slots']) if row['name'] == 'Mat_Bokusei_Face')
                for actor in game_models[6:8]:
                    p = actor.toon_presentation
                    c = actor.skeletal_mesh_component
                    mid = c.get_material(face_index)
                    assert isinstance(mid,unreal.MaterialInstanceDynamic) and mid.get_scalar_parameter_value('FaceSDFEnabled') == 1
                    expected = unreal.MathLibrary.transform_direction(c.get_socket_transform(p.head_bone),p.head_forward_axis)
                    actual = mid.get_vector_parameter_value('HeadForwardWS')
                    # RefreshPresentation caches each axis with FVector::Equals(.0001).
                    # Match its component-wise tolerance instead of imposing a smaller
                    # Euclidean tolerance when all three coordinates changed slightly.
                    forward_error = max(abs(x-y) for x,y in zip([actual.r,actual.g,actual.b],vec(expected)))
                    assert forward_error < 1.01e-4, ('SDF head forward drift',forward_error,actor.get_actor_label())
                    expected_right = unreal.MathLibrary.transform_direction(c.get_socket_transform(p.head_bone),p.head_right_axis)
                    actual_right = mid.get_vector_parameter_value('HeadRightWS')
                    right_error = max(abs(x-y) for x,y in zip([actual_right.r,actual_right.g,actual_right.b],vec(expected_right)))
                    assert right_error < 1.01e-4, ('SDF head right drift',right_error,actor.get_actor_label())
                REPORT['sdf_head_tracking'] = dict(status='PASS',animated=True,materials=2)
            hair_pose_error = 0
            for model in game_models[1:]:
                proxy = model.hair_shadow_proxy
                if not proxy.get_skeletal_mesh_asset(): continue
                assert proxy.get_editor_property('leader_pose_component') == a
                visible = model.skeletal_mesh_component
                hair_pose_error = max(hair_pose_error, max(math.dist(vec(proxy.get_socket_location(name)), vec(visible.get_socket_location(name))) for name in sample))
            assert hair_pose_error < .001, ('Hair shadow pose drift', hair_pose_error)
            if 'pie' in REPORT:
                test_inputs(game_world, now)
                return
            if first is None:
                first = sample
                last = now
                expected = next(c for c in unreal.GameplayStatics.get_all_actors_of_class(game_world, unreal.CameraActor) if c.actor_has_tag('PGShadingOverview'))
                assert math.dist(vec(pawn.get_actor_location()), vec(expected.get_actor_location())) < .01
                assert unreal.PGToonPreviewActor.set_preview_viewport_size(game_world, 1280, 720)
                return
            if now-last < .6:
                return
            movement = max(math.dist(sample[name], first[name]) for name in sample)
            assert movement > .001, ('PIE idle did not animate', movement)
            REPORT['pie'] = dict(status='PASS', matched_bones=len(errors), pose_max_error_cm=max(errors), movement_cm=movement,
                                 hair_pose_max_error_cm=hair_pose_error, models=STAGES, camera_pawns=1)
            return
        if not shots:
            if now-started < 12: return
            for actor in models[1:]: actor.toon_presentation.initialize(None)
            left.override_animation_data(idle, True, True, 1.25, 1)
            pie_started = now
            level.editor_request_begin_play()
            return
        if now-started < 25 or now-last < 3:
            return
        if pending:
            if not pending.is_file() or pending.stat().st_size < 10000:
                return
            # A first offscreen high-resolution frame can precede translucent PSO
            # readiness; discard it before collecting the actual comparison views.
            if shots[index][0] != 'Warmup':
                REPORT['images'].append(str(pending))
            pending, prepared, last = None, False, now
            index += 1
            if index == len(shots):
                REPORT['editor_render'] = dict(status='PASS', images=len(REPORT['images']), shadow_comparison=True)
                if SDF_PROBE: REPORT['face_sdf']['status'] = 'PASS'
                # Restore in-memory source interfaces before BeginPlay reinitializes.
                for caster in shadow_casters: caster.static_mesh_component.set_cast_shadow(False)
                hair_proxy.set_cast_shadow(True)
                if STAGES == 8: models[7].hair_shadow_proxy.set_cast_shadow(True)
                key_light.set_actor_rotation(key_rotation,False)
                for actor in models[1:]: actor.toon_presentation.initialize(None)
                left.override_animation_data(idle, True, True, 1.25, 1)
                pie_started = now
                level.editor_request_begin_play()
            return
        shot = shots[index]
        name, camera = shot[:2]
        if not prepared:
            for caster in shadow_casters: caster.static_mesh_component.set_cast_shadow(shot[4] if len(shot) > 4 else False)
            hair_proxy.set_cast_shadow(shot[5] if len(shot) > 5 else True)
            if STAGES == 8: models[7].hair_shadow_proxy.set_cast_shadow(shot[5] if len(shot) > 5 else True)
            key_light.set_actor_rotation(shot[6] if len(shot) > 6 else key_rotation,False)
            for actor in models[1:]: actor.toon_presentation.refresh_presentation()
            loc, rot = (camera.get_actor_location(), camera.get_actor_rotation()) if camera else shot[2:4]
            level.set_level_viewport_camera_info(loc, rot, 'None')
            level.set_level_viewport_fov(45, 'None')
            prepared, last = True, now
            return
        pending = RUN/(name+'.png')
        unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1600x900 filename="{pending.as_posix()}"')
        last = now
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

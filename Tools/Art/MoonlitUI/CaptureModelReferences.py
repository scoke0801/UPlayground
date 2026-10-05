"""Capture actual selectable meshes/materials in a transient UE world, without saves."""
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/MoonlitUI/ModelReferences'
OUT.mkdir(parents=True, exist_ok=True)
NAMES = ['Bokusei', 'LianLian', 'Honoka', 'Hichi', 'Siuha', 'Lili', 'Nenmir']
REPORT = dict(status='RUNNING', characters=[], images=[])
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
pp = ACTORS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
pp.set_editor_property('unbound', True)
settings = unreal.PostProcessSettings()
for key, value in dict(override_auto_exposure_method=True, auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
    override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
    override_auto_exposure_bias=True, auto_exposure_bias=0., override_bloom_intensity=True, bloom_intensity=0.,
    override_motion_blur_amount=True, motion_blur_amount=0.).items():
    settings.set_editor_property(key, value)
pp.set_editor_property('settings', settings)
key = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,300), unreal.Rotator(pitch=-35,yaw=-75))
key.light_component.set_intensity(3.2)
key.light_component.set_cast_shadows(False)
fill = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,300), unreal.Rotator(pitch=-20,yaw=100))
fill.light_component.set_intensity(1.2)
fill.light_component.set_cast_shadows(False)
bases = {}
for i, name in enumerate(NAMES):
    appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_' + name)
    mesh = appearance.get_editor_property('mesh')
    actor = ACTORS.spawn_actor_from_class(unreal.PGToonPreviewActor, unreal.Vector(i*500,0,0))
    comp = actor.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(mesh)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    toon = actor.toon_presentation
    for prop in ('head_bone', 'head_forward_axis', 'head_right_axis'):
        toon.set_editor_property(prop, appearance.get_editor_property(prop))
    toon.set_editor_property('key_light', key)
    toon.initialize(comp)
    bases[name] = actor
    imported = list(mesh.get_editor_property('asset_import_data').extract_filenames())
    REPORT['characters'].append(dict(name=name, mesh=mesh.get_path_name(), imported_sources=imported,
        materials=[comp.get_material(j).get_path_name() for j in range(comp.get_num_materials())]))
camera = ACTORS.spawn_actor_from_class(unreal.CameraActor, unreal.Vector())
camera.camera_component.set_editor_property('projection_mode', unreal.CameraProjectionMode.ORTHOGRAPHIC)
camera.camera_component.set_editor_property('aspect_ratio', 1.)
LEVEL.editor_set_game_view(True)
for cmd in ('DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.Streaming.FullyLoadUsedTextures 1', 'viewmode lit'):
    unreal.SystemLibrary.execute_console_command(world, cmd)
shots = [(n, v) for n in NAMES for v in ('Front', 'Face')]
index, prepared, pending = 0, False, None
started = last = time.monotonic()

def finish(error=None):
    REPORT['status'] = 'FAIL' if error else 'PASS'
    if error: REPORT['error'] = error
    try:
        (OUT / 'capture.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
    finally:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()

def tick(dt):
    global index, prepared, pending, last
    try:
        now = time.monotonic()
        if now-started > 480: return finish('Capture timeout')
        if now-started < 25 or now-last < 2: return
        if pending:
            if not pending.exists() or pending.stat().st_size < 10000: return
            REPORT['images'].append(str(pending))
            index += 1
            prepared, pending, last = False, None, now
            if index == len(shots): return finish()
            return
        name, view = shots[index]
        if prepared:
            pending = OUT / (name + '_' + view + '.png')
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1536x1536 filename="' + pending.as_posix() + '"')
            last = now
            return
        actor = bases[name]
        for n, a in bases.items(): a.set_is_temporarily_hidden_in_editor(n != name)
        head = actor.skeletal_mesh_component.get_socket_location(actor.toon_presentation.head_bone)
        origin, extent = actor.get_actor_bounds(False)
        # Frame from actual head height; imported invisible bounds can extend below feet.
        height = head.z - actor.get_actor_location().z
        target = head + unreal.Vector(0,0,-height*.25)
        width = height*1.38
        if view == 'Face':
            target = head + unreal.Vector(0,0,4)
            width = height*.48
        loc = target + unreal.Vector(0,500,0)
        camera.set_actor_location(loc,False,False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(loc,target),False)
        camera.camera_component.set_editor_property('ortho_width',width)
        LEVEL.pilot_level_actor(camera,'None')
        prepared,last = True,now
    except Exception: finish(traceback.format_exc())

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)

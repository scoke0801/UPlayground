"""Persist/capture four-character toon gallery; never edits the Inori motion map."""
import json
import re
import shutil
import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
import ConfigureToonCharacterTest as shared

imports = json.loads((ROOT / 'Saved/ToonTest/additional_import.json').read_text(encoding='utf-8'))
assert imports['status'] == 'PASS'
specs = json.loads((ROOT / 'Tools/Art/ToonTest/characters.json').read_text(encoding='utf-8'))
ground_offsets = {row['name']: row.get('gallery_z_adjust_cm', 0) for row in specs['characters']}
OUT = ROOT / 'Saved/ToonTest/Gallery' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'images': [], 'characters': []}
LATEST = ROOT / 'Saved/ToonTest/gallery.json'
MAP = '/Game/Art/ToonTest/Maps/L_PGToon_CharacterGallery'
REPORT['map'] = MAP
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
assert world
world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
pp.set_editor_property('unbound', True)
settings = unreal.PostProcessSettings()
for key, value in {'override_auto_exposure_method': True, 'auto_exposure_method': unreal.AutoExposureMethod.AEM_MANUAL,
                   'override_auto_exposure_apply_physical_camera_exposure': True, 'auto_exposure_apply_physical_camera_exposure': False,
                   'override_auto_exposure_bias': True, 'auto_exposure_bias': 0.,
                   'override_bloom_intensity': True, 'bloom_intensity': 0.}.items():
    settings.set_editor_property(key, value)
pp.set_editor_property('settings', settings)
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -6))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.static_mesh_component.set_material(0, unreal.load_asset('/Game/Art/ToonTest/Materials/M_PGToonPreviewFloor'))
floor.set_actor_scale3d(unreal.Vector(18, 12, .1))
light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 250), unreal.Rotator(pitch=-55, yaw=-90, roll=0))
light.light_component.set_editor_property('intensity', 3.)
light.light_component.set_editor_property('cast_shadows', False)

# Load the finished graph. Editing a live graph on every capture generates
# transient missing-input compile warnings while its connections are rebuilt.
outline = unreal.load_asset('/Game/Art/ToonTest/Materials/M_PGToonOutlineMulti')
if not outline:
    outline = shared.build_outline_master('M_PGToonOutlineMulti', extended_alpha=True)
inori_outline = json.loads((ROOT / 'Saved/ToonTest/outline.json').read_text(encoding='utf-8'))
entries = [{'name': 'Inori', 'mesh': '/Game/Art/ToonTest/Inori/SK_Inori_ToonTest', 'slots': []}] + imports['characters']
base_actors, hull_actors, labels = [], [], []
for index, entry in enumerate(entries):
    name = entry['name']
    mesh = unreal.load_asset(entry['mesh'])
    assert isinstance(mesh, unreal.SkeletalMesh)
    location = unreal.Vector((index-1.5)*300, 0, 0)
    base = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, location)
    base.set_actor_label('PG Toon Gallery ' + name)
    component = base.skeletal_mesh_component
    component.set_skeletal_mesh_asset(mesh)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property('cast_shadow', False)
    # Keep the requested additions as reference-pose import tests.
    if name == 'Inori':
        for slot_index, slot in enumerate(mesh.get_editor_property('materials')):
            override = inori_outline.get('overlays', {}).get(str(slot.material_slot_name))
            if override:
                component.set_material(slot_index, unreal.load_asset(override))
        anim = unreal.load_asset('/Game/Art/ToonTest/Inori/Animation/PGToon_AS_idle')
        data = unreal.SingleAnimationPlayData()
        data.set_editor_property('anim_to_play', anim)
        data.set_editor_property('saved_position', .5)
        component.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        component.set_editor_property('animation_data', data)
        component.set_animation(anim)
        component.set_position(.5, False)
        component.set_update_animation_in_editor(True)
    origin, extent = base.get_actor_bounds(False)
    # Honoka has non-visible bounds below its boots; the measured adjustment is
    # explicit in the manifest, not a gameplay mesh/scale modification.
    ground_offset = ground_offsets.get(name, 0)
    base.set_actor_location(location + unreal.Vector(0, 0, -(origin.z-extent.z)+ground_offset), False, False)
    origin, extent = base.get_actor_bounds(False)
    hull = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, base.get_actor_location())
    hull.set_actor_label('PG Toon Gallery ' + name + ' Outline')
    hc = hull.skeletal_mesh_component
    hc.set_skeletal_mesh_asset(mesh)
    hc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    hc.set_editor_property('cast_shadow', False)
    hc.set_leader_pose_component(component)
    for slot_index, slot in enumerate(mesh.get_editor_property('materials')):
        if name == 'Inori':
            hc.set_material(slot_index, unreal.load_asset(inori_outline['instances'][slot_index]['asset']))
            continue
        config = entry['slots'][slot_index]
        slot_name = config['slot']
        mi_name = 'MI_PGToonOutline_' + name + '_' + re.sub(r'[^A-Za-z0-9_]', '_', slot_name)
        dest = '/Game/Art/ToonTest/' + name + '/Materials'
        mi_path = dest + '/' + mi_name
        mi = unreal.load_asset(mi_path) if unreal.EditorAssetLibrary.does_asset_exist(mi_path) else tools.create_asset(mi_name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(mi, outline)
        base_mat = component.get_material(slot_index)
        for param in ['BaseTexture', 'OpacityTexture']:
            tex = lib.get_material_instance_texture_parameter_value(base_mat, param)
            if tex:
                lib.set_material_instance_texture_parameter_value(mi, param, tex)
        for param in ['AlphaMaskMode', 'AlphaMaskScale', 'AlphaMaskValue', 'OpacityCutoff', 'UseBaseAlpha', 'MainOpacity', 'DissolveAmount']:
            lib.set_material_instance_scalar_parameter_value(mi, param, lib.get_material_instance_scalar_parameter_value(base_mat, param))
        face = any(word in slot_name.lower() for word in ['face', 'eye', 'brow', 'trans', 'tears'])
        lib.set_material_instance_scalar_parameter_value(mi, 'OutlineWidth', .08 if face else (.18 if 'hair' in slot_name.lower() else .3))
        lib.set_material_instance_scalar_parameter_value(mi, 'OutlineEnabled', 0 if config['transparent'] or face else 1)
        shared.save(mi)
        hc.set_material(slot_index, mi)
    label = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(location.x, 85, 5), unreal.Rotator(pitch=55, yaw=90, roll=0))
    label.text_render.set_text(name)
    label.text_render.set_world_size(25)
    label.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    base_actors.append(base)
    hull_actors.append(hull)
    labels.append(label)
    REPORT['characters'].append({'name': name, 'mesh': mesh.get_path_name(), 'material_slots': component.get_num_materials(),
                                  'height_cm': extent.z*2, 'center': [origin.x, origin.y, origin.z],
                                  'gallery_z_adjust_cm': ground_offset,
                                  'pose': 'Idle' if name == 'Inori' else 'Reference Pose'})

aim = unreal.Vector(0, 0, 85)
camera_location = unreal.Vector(0, 1050, 230)
camera_rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, aim)
camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_location, camera_rotation)
camera.set_actor_label('PG Toon Gallery Camera')
camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
camera.camera_component.set_editor_property('field_of_view', 60)
level.set_level_viewport_fov(60, 'None')
level.set_level_viewport_camera_info(camera_location, camera_rotation, 'None')
level.editor_set_game_view(True)
map_file = ROOT / 'Content/Art/ToonTest/Maps/L_PGToon_CharacterGallery.umap'
if map_file.is_file():
    shutil.copy2(map_file, OUT / 'previous_map.umap')
assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
base_actors[0].skeletal_mesh_component.set_play_rate(0)
for command in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.Streaming.FullyLoadUsedTextures 1', 'viewmode lit']:
    unreal.SystemLibrary.execute_console_command(world, command)
# Capture the overview last, after every character has rendered at close range.
shots = [(entry['name']+'_Close', i) for i, entry in enumerate(entries) if i > 0] + [('AllCharacters', None)]
index, pending, prepared = 0, None, False
started = last = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def finish(error=None):
    REPORT['status'] = 'FAIL' if error else 'CAPTURED'
    REPORT['visual_review_required'] = True
    if error:
        REPORT['error'] = error
        unreal.log_error(error)
    payload = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (OUT / 'gallery.json').write_text(payload, encoding='utf-8')
    LATEST.write_text(payload, encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global index, pending, last, prepared
    try:
        now = time.monotonic()
        if now-started > 240:
            finish('Gallery render timeout')
            return
        if now-started < 60 or now-last < 10:
            return
        if pending:
            if not pending.is_file() or pending.stat().st_size < 10000:
                return
            REPORT['images'].append(str(pending))
            index += 1
            pending = None
            prepared = False
            last = now
            if index == len(shots):
                finish()
            return
        name, actor_index = shots[index]
        if prepared:
            pending = OUT / (name+'.png')
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x720 filename="{pending.as_posix()}"')
            last = now
            return
        for i, actor in enumerate(base_actors):
            hidden = actor_index is not None and actor_index != i
            actor.set_is_temporarily_hidden_in_editor(hidden)
            hull_actors[i].set_is_temporarily_hidden_in_editor(hidden)
            labels[i].set_is_temporarily_hidden_in_editor(hidden)
        if actor_index is None:
            loc, rot = camera_location, camera_rotation
        else:
            bounds = REPORT['characters'][actor_index]
            center = unreal.Vector(*bounds['center'])
            distance = max(330, bounds['height_cm']*2.05)
            loc = center + unreal.Vector(distance*.12, distance, 10)
            rot = unreal.MathLibrary.find_look_at_rotation(loc, center)
        level.set_level_viewport_camera_info(loc, rot, 'None')
        prepared = True
        last = now
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

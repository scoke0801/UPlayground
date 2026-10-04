"""Save a playable lighting fixture, render real shadows and head/light response."""
import json
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
DATA = json.loads((ROOT / 'Saved/ToonTest/lighting_lab.json').read_text())
assert DATA['status'] == 'PASS'
OUT = ROOT / 'Saved/ToonTest/LightingPreview' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'images': [], 'characters': []}
LATEST = ROOT / 'Saved/ToonTest/lighting_preview.json'
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')
MAP = '/Game/Art/ToonTest/Maps/L_PGToon_LightingLab'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property('default_game_mode', unreal.load_asset(DATA['game_mode']).generated_class())
unreal.SystemLibrary.execute_console_command(world, 'r.CustomDepth 3')

pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
pp.set_editor_property('unbound', True)
settings = unreal.PostProcessSettings()
for k, v in {'override_auto_exposure_method': True, 'auto_exposure_method': unreal.AutoExposureMethod.AEM_MANUAL,
             'override_auto_exposure_apply_physical_camera_exposure': True, 'auto_exposure_apply_physical_camera_exposure': False,
             'override_auto_exposure_bias': True, 'auto_exposure_bias': 0., 'override_bloom_intensity': True, 'bloom_intensity': 0.,
             'override_motion_blur_amount': True, 'motion_blur_amount': 0.}.items():
    settings.set_editor_property(k, v)
pp.set_editor_property('settings', settings)
pp.add_or_update_blendable(unreal.load_asset(DATA['outline']), 1)

shared.MASTER_DEST = '/Game/Art/ToonTest/Advanced/Materials'
floor_mat = shared.recreate_material('M_PGToonLabFloor')
floor_mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
color = shared.vector_parameter(floor_mat, 'Color', (.14, .17, .22, 1), -200, 0)
unreal.MaterialEditingLibrary.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
rough = shared.scalar_parameter(floor_mat, 'Roughness', 1, -200, 100)
unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
unreal.MaterialEditingLibrary.recompile_material(floor_mat)
shared.save(floor_mat)
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -6))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.static_mesh_component.set_material(0, floor_mat)
floor.set_actor_scale3d(unreal.Vector(22, 18, .1))
key = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(pitch=-55, yaw=-90, roll=0))
key.set_actor_label('PG Toon Key Light')
key.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
key.light_component.set_intensity(3.2)
key.light_component.set_editor_property('light_source_angle', 3.)
fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(pitch=-35, yaw=70, roll=0))
fill.set_actor_label('PG Toon Fill Light')
fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
fill.light_component.set_intensity(.45)
fill.light_component.set_cast_shadows(False)
occluder = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 45, 195))
occluder.set_actor_label('PG Toon Shadow Occluder')
occluder.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
occluder.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
occluder.static_mesh_component.set_material(0, floor_mat)
occluder.set_actor_scale3d(unreal.Vector(1.1, .7, .15))
occluder.set_is_temporarily_hidden_in_editor(True)
occluder.set_actor_hidden_in_game(True)

bases, ref_heads = {}, {}
for i, row in enumerate(DATA['characters']):
    name = row['name']
    actor = actors.spawn_actor_from_class(unreal.PGToonPreviewActor, unreal.Vector((i-1.5)*260, 0, 0))
    actor.set_actor_label('PG Toon Lab ' + name)
    c = actor.skeletal_mesh_component
    mesh = unreal.load_asset(row['mesh'])
    c.set_skeletal_mesh_asset(mesh)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_render_custom_depth(True)
    c.set_custom_depth_stencil_value(73)
    c.set_editor_property('cast_shadow', True)
    for index, material in enumerate(row['materials']):
        c.set_material(index, unreal.load_asset(material['asset']))
    bone = next((c.get_bone_name(j) for j in range(c.get_num_bones())
                 if str(c.get_bone_name(j)).lower() in ['head', 'head_x', 'j_bip_c_head']), None)
    assert bone, name
    rig = unreal.IKRigDefinition()
    ctl = unreal.IKRigController.get_controller(rig)
    ctl.set_skeletal_mesh(mesh)
    ref = ctl.get_ref_pose_transform_of_bone(bone)
    ref_heads[name] = ref.translation
    presentation = actor.toon_presentation
    presentation.set_editor_property('key_light', key)
    presentation.set_editor_property('head_bone', bone)
    presentation.set_editor_property('head_forward_axis', unreal.MathLibrary.inverse_transform_direction(ref, unreal.Vector(0, 1, 0)))
    presentation.set_editor_property('head_right_axis', unreal.MathLibrary.inverse_transform_direction(ref, unreal.Vector(1, 0, 0)))
    if name == 'Inori':
        anim = unreal.load_asset('/Game/Art/ToonTest/Inori/Animation/PGToon_AS_idle')
        data = unreal.SingleAnimationPlayData()
        data.anim_to_play, data.saved_position = anim, .5
        c.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        c.set_editor_property('animation_data', data)
        c.set_animation(anim)
        c.set_position(.5, False)
        c.set_update_animation_in_editor(True)
    origin, extent = actor.get_actor_bounds(False)
    actor.set_actor_location(actor.get_actor_location() + unreal.Vector(0, 0, -(origin.z-extent.z) + (-23.5 if name == 'Honoka' else 0)), False, False)
    bases[name] = actor
    REPORT['characters'].append({'name': name, 'head_bone': str(bone), 'slots': c.get_num_materials()})

cam_loc = unreal.Vector(0, 1050, 280)
cam_rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, unreal.Vector(0, 0, 85))
camera = actors.spawn_actor_from_class(unreal.CameraActor, cam_loc, cam_rot)
camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
camera.camera_component.set_editor_property('field_of_view', 60)
map_file = ROOT / 'Content/Art/ToonTest/Maps/L_PGToon_LightingLab.umap'
if map_file.is_file():
    shutil.copy2(map_file, OUT / 'previous_map.umap')
assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
REPORT['map'] = MAP
for actor in bases.values():
    actor.toon_presentation.initialize(actor.skeletal_mesh_component)
bases['Inori'].skeletal_mesh_component.set_play_rate(0)
ref_actor = actors.spawn_actor_from_class(unreal.PGToonPreviewActor, bases['Inori'].get_actor_location())
ref_actor.skeletal_mesh_component.set_skeletal_mesh_asset(bases['Inori'].skeletal_mesh_component.get_skeletal_mesh_asset())
ref_actor.skeletal_mesh_component.set_render_custom_depth(True)
ref_actor.skeletal_mesh_component.set_custom_depth_stencil_value(73)
for i, row in enumerate(DATA['characters'][0]['materials']):
    ref_actor.skeletal_mesh_component.set_material(i, unreal.load_asset(row['asset']))
for prop in ['key_light', 'head_bone', 'head_forward_axis', 'head_right_axis']:
    ref_actor.toon_presentation.set_editor_property(prop, bases['Inori'].toon_presentation.get_editor_property(prop))
ref_actor.toon_presentation.initialize(ref_actor.skeletal_mesh_component)
ref_actor.set_is_temporarily_hidden_in_editor(True)
for command in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.Streaming.FullyLoadUsedTextures 1', 'viewmode lit']:
    unreal.SystemLibrary.execute_console_command(world, command)
level.editor_set_game_view(True)
shots = [(name+'_Lit', name, 'portrait') for name in bases]
shots += [('Honoka_Shadow', 'Honoka', 'shadow'), ('Honoka_Occluded', 'Honoka', 'occluded'), ('Honoka_NoKey', 'Honoka', 'dark'),
          ('Bokusei_Side', 'Bokusei', 'side'), ('Inori_Quarter', 'Inori', 'quarter'),
          ('Inori_Reference', 'Inori', 'reference'), ('Inori_State', 'Inori', 'state'),
          ('Inori_Dissolved', 'Inori', 'dissolved'),
          ('Honoka_Quarter', 'Honoka', 'quarter'), ('AllCharacters', None, 'all')]
index, pending, prepared = 0, None, False
started = last = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def finish(error=None):
    REPORT.update(status='FAIL' if error else 'CAPTURED', visual_review_required=True)
    if error:
        REPORT['error'] = error
        unreal.log_error(error)
    text = json.dumps(REPORT, indent=2)
    LATEST.write_text(text, encoding='utf-8')
    (OUT / 'capture.json').write_text(text, encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global index, pending, prepared, last
    try:
        now = time.monotonic()
        if now-started > 420:
            finish('Lighting render timeout')
            return
        if now-started < 35 or now-last < 3:
            return
        if pending:
            if not pending.is_file() or pending.stat().st_size < 10000:
                return
            REPORT['images'].append(str(pending))
            index += 1
            pending, prepared, last = None, False, now
            if index == len(shots):
                finish()
            return
        label, name, mode = shots[index]
        if prepared:
            pending = OUT / (label+'.png')
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x720 filename="{pending.as_posix()}"')
            last = now
            return
        for n, actor in bases.items():
            actor.set_is_temporarily_hidden_in_editor(mode == 'reference' or (name is not None and n != name))
        ref_actor.set_is_temporarily_hidden_in_editor(mode != 'reference')
        key.light_component.set_intensity(0 if mode == 'dark' else 3.2)
        fill.light_component.set_intensity(0 if mode == 'dark' else .45)
        key.set_actor_rotation(unreal.Rotator(pitch=-25 if mode == 'side' else -55, yaw=-165 if mode == 'side' else -90, roll=0), False)
        occluder.set_is_temporarily_hidden_in_editor(mode not in ['shadow', 'occluded'])
        occluder.set_actor_hidden_in_game(mode not in ['shadow', 'occluded'])
        occluder.set_actor_scale3d(unreal.Vector(1.1, .1, .9) if mode == 'occluded' else unreal.Vector(1.1, .7, .15))
        inori = bases['Inori'].skeletal_mesh_component
        inori.set_position(.5, False)
        inori.set_play_rate(0)
        for slot in range(inori.get_num_materials()):
            mid = inori.get_material(slot)
            mid.set_scalar_parameter_value('DissolveAmount', 1. if mode == 'dissolved' else 0.)
            mid.set_scalar_parameter_value('StateGlow', .7 if mode == 'state' else 0.)
            mid.set_vector_parameter_value('StateColor', unreal.LinearColor(.2, .65, 1., 1.))
        if name:
            actor = bases[name]
            head = (actor.skeletal_mesh_component.get_socket_location(actor.toon_presentation.head_bone) if name == 'Inori'
                    else actor.get_actor_location() + ref_heads[name])
            aim = head + unreal.Vector(0, 0, 2)
            loc = aim + unreal.Vector(12, 155, 3)
            fov = 40
            occluder.set_actor_location(unreal.Vector(head.x, 45, head.z+35), False, False)
            if mode == 'occluded':
                occluder.set_actor_location(head + unreal.Vector(0, 70, -15), False, False)
            if mode in ['quarter', 'reference', 'state', 'dissolved']:
                aim = unreal.Vector(actor.get_actor_location().x, 0, 78)
                loc = aim + unreal.Vector(240, 310, 390)
        else:
            loc, aim, fov = cam_loc, unreal.Vector(0, 0, 85), 60
        level.set_level_viewport_fov(fov, 'None')
        level.set_level_viewport_camera_info(loc, unreal.MathLibrary.find_look_at_rotation(loc, aim), 'None')
        for actor in bases.values():
            actor.toon_presentation.refresh_presentation()
        prepared, last = True, now
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

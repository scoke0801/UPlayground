"""Capture repeatable close/quarter/light tests from the saved gallery, without saving it.

PG_TOON_CAPTURE_TAG selects an output label (e.g. before/after); every run is unique.
Lighting variations use transient MIDs, never edit the saved material instances.
"""
import json
import os
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
TAG = os.environ.get('PG_TOON_CAPTURE_TAG', 'after')
NO_HULL = os.environ.get('PG_TOON_HIDE_HULLS') == '1'
OUT = ROOT / 'Saved/ToonTest/ShadingPreview' / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '_' + TAG)
OUT.mkdir(parents=True)
LATEST = ROOT / 'Saved/ToonTest' / ('shading_preview_' + TAG + '.json')
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'images': [], 'tag': TAG}
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Art/ToonTest/Maps/L_PGToon_CharacterGallery')
assert world
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
bases = {a.get_actor_label().removeprefix('PG Toon Gallery '): a for a in actors
         if isinstance(a, unreal.SkeletalMeshActor) and not a.get_actor_label().endswith(' Outline')}
hulls = {a.get_actor_label().removeprefix('PG Toon Gallery ').removesuffix(' Outline'): a for a in actors
         if isinstance(a, unreal.SkeletalMeshActor) and a.get_actor_label().endswith(' Outline')}
mids = []
for name, actor in bases.items():
    c = actor.skeletal_mesh_component
    if name == 'Inori':
        c.set_position(.5, False)
        c.set_play_rate(0)
    for i in range(c.get_num_materials()):
        mids.append(c.create_dynamic_material_instance(i))
for a in actors:
    if isinstance(a, unreal.TextRenderActor):
        a.set_is_temporarily_hidden_in_editor(True)
for command in ['DisableAllScreenMessages', 'r.ScreenPercentage 100', 'r.Streaming.FullyLoadUsedTextures 1', 'viewmode lit']:
    unreal.SystemLibrary.execute_console_command(world, command)
level.editor_set_game_view(True)
level.set_level_viewport_fov(40, 'None')
# Explicit visible head positions; Honoka's non-visible FBX bounds are unsuitable for framing.
heads = {'Inori': 151, 'Bokusei': 143, 'Honoka': 132, 'LianLian': 143}
shots = [(n + '_Portrait', n, 'portrait', (.35, -.45, -.82)) for n in bases]
shots += [('Bokusei_Quarter', 'Bokusei', 'quarter', (.35, -.45, -.82)),
          ('Bokusei_SideLight', 'Bokusei', 'portrait', (-.9, -.15, -.35)),
          ('Bokusei_BackLight', 'Bokusei', 'portrait', (.25, .9, -.35)),
          ('Honoka_Face', 'Honoka', 'head', (.35, -.45, -.82)),
          ('Inori_Quarter', 'Inori', 'quarter', (.35, -.45, -.82))]
if NO_HULL:
    shots = [shot for shot in shots if shot[0] in ['Inori_Portrait', 'Inori_Quarter', 'LianLian_Portrait']]
REPORT['hulls_disabled'] = NO_HULL
index, pending, prepared = 0, None, False
started = last = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def finish(error=None):
    REPORT.update(status='FAIL' if error else 'CAPTURED', visual_review_required=True)
    if error:
        REPORT['error'] = error
        unreal.log_error(error)
    payload = json.dumps(REPORT, indent=2)
    LATEST.write_text(payload, encoding='utf-8')
    (OUT / 'capture.json').write_text(payload, encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global index, pending, prepared, last
    try:
        now = time.monotonic()
        if now - started > 360:
            finish('Toon shading capture timeout')
            return
        if now - started < 45 or now - last < 5:
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
        label, name, framing, light = shots[index]
        if prepared:
            pending = OUT / (label + '.png')
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x720 filename="{pending.as_posix()}"')
            last = now
            return
        for n, actor in bases.items():
            actor.set_is_temporarily_hidden_in_editor(n != name)
            hulls[n].set_is_temporarily_hidden_in_editor(NO_HULL or n != name)
        for mi in mids:
            mi.set_vector_parameter_value('LightDirection', unreal.LinearColor(*light, 0))
        actor = bases[name]
        origin = actor.get_actor_location()
        aim = unreal.Vector(origin.x, 0, heads[name] - 9)
        offset = unreal.Vector(12, 122, 0)
        if framing == 'head':
            component = actor.skeletal_mesh_component
            bone = next((component.get_bone_name(i) for i in range(component.get_num_bones())
                         if str(component.get_bone_name(i)).lower() == 'head'), None)
            aim = component.get_socket_location(bone) + unreal.Vector(0, 0, 3) if bone else unreal.Vector(origin.x, 0, 160)
            offset = unreal.Vector(8, 145, 0)
        if framing == 'quarter':
            aim = unreal.Vector(origin.x, 0, 78)
            offset = unreal.Vector(240, 310, 390)
        loc = aim + offset
        level.set_level_viewport_camera_info(loc, unreal.MathLibrary.find_look_at_rotation(loc, aim), 'None')
        prepared, last = True, now
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

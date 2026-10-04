"""Render the isolated Inori toon conversion in an unsaved editor world."""

import json
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/ToonTest/Preview"
OUT.mkdir(parents=True, exist_ok=True)
REPORT = OUT / "preview.json"
IMAGE = OUT / "Inori_Toon.png"
REPORT.write_text('{"status":"RUNNING"}', encoding="utf-8")

mesh = unreal.load_asset("/Game/Art/ToonTest/Inori/SK_Inori_ToonTest")
outline = unreal.load_asset("/Game/Art/ToonTest/Materials/M_PGToonOutline")
if not isinstance(mesh, unreal.SkeletalMesh) or not isinstance(outline, unreal.Material):
    raise RuntimeError("Run ConfigureToonCharacterTest.py before the preview")

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if not world:
    raise RuntimeError("Could not create an unsaved preview world")

outline_actor = actors.spawn_actor_from_class(
    unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
)
base_actor = actors.spawn_actor_from_class(
    unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
)
outline_component = outline_actor.get_editor_property("skeletal_mesh_component")
base_component = base_actor.get_editor_property("skeletal_mesh_component")
outline_component.set_skeletal_mesh_asset(mesh)
base_component.set_skeletal_mesh_asset(mesh)
outline_component.set_editor_property("cast_shadow", False)
for index in range(outline_component.get_num_materials()):
    outline_component.set_material(index, outline)

origin, extent = base_actor.get_actor_bounds(False)
height = max(extent.z * 2.0, 100.0)
distance = max(height * 1.05, 210.0)
target = origin + unreal.Vector(0, 0, height * 0.03)
camera_location = target + unreal.Vector(distance * 0.18, distance, height * 0.08)
camera_rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, target)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera_location, camera_rotation)

unreal.SystemLibrary.execute_console_command(world, "DisableAllScreenMessages")
unreal.SystemLibrary.execute_console_command(world, "r.ViewDistanceScale 1")
unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 100")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

started = time.monotonic()
shot_requested = False
finished = False


def finish(error=None):
    global finished
    if finished:
        return
    finished = True
    payload = {
        "status": "FAIL" if error else "PASS",
        "image": str(IMAGE),
        "mesh": mesh.get_path_name(),
        "material_slots": base_component.get_num_materials(),
        "bounds_origin": str(origin),
        "bounds_extent": str(extent),
        "camera_location": str(camera_location),
        "camera_rotation": str(camera_rotation),
    }
    if error:
        payload["error"] = error
        unreal.log_error("PG Toon preview failed: " + error)
    else:
        unreal.log("PG Toon preview passed: " + json.dumps(payload))
    REPORT.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_delta_seconds):
    global shot_requested
    try:
        elapsed = time.monotonic() - started
        if elapsed > 40:
            finish("Timed out while rendering")
        elif not shot_requested and elapsed > 12:
            unreal.SystemLibrary.execute_console_command(
                world, f'HighResShot 1280x720 filename="{IMAGE.as_posix()}"'
            )
            shot_requested = True
        elif shot_requested and elapsed > 18:
            if not IMAGE.is_file():
                finish("HighResShot did not produce the requested image")
            else:
                finish()
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

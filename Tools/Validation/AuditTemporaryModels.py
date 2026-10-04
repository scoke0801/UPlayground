"""Read-only audit of primitive/prototype meshes used by UPlayground maps.

Run with UnrealEditor-Cmd and the PythonScriptPlugin. The report is written to
Saved/TemporaryModels/audit.json; no packages are saved.
"""

import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved" / "TemporaryModels"
OUT.mkdir(parents=True, exist_ok=True)

MAPS = [
    "/Game/Maps/DummyDevMap",
    "/Game/Maps/EmptyDevMap",
    "/Game/Maps/EnemyDevMap",
    "/Game/Maps/FeatureDevMap",
    "/Game/Maps/StageDevMap",
    "/Game/Maps/RogueArena",
]

AUDITED_MESHES = [
    "/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_Cube",
    "/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_ChamferCube",
    "/Engine/BasicShapes/Cylinder",
]


def vector(value):
    return [round(value.x, 5), round(value.y, 5), round(value.z, 5)]


def mesh_info(path):
    mesh = unreal.load_asset(path)
    assert isinstance(mesh, unreal.StaticMesh), path
    bounds = mesh.get_bounding_box()
    return {
        "path": mesh.get_path_name(),
        "bounds_cm": [vector(bounds.min), vector(bounds.max)],
        "triangles": mesh.get_num_triangles(0),
        "material_slots": [
            str(slot.get_editor_property("material_slot_name"))
            for slot in mesh.get_editor_property("static_materials")
        ],
    }


report = {
    "meshes": {path: mesh_info(path) for path in AUDITED_MESHES},
    "maps": {},
}

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for map_path in MAPS:
    assert level.load_level(map_path), map_path
    references = []
    for actor in actors.get_all_level_actors():
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            continue
        mesh = component.get_editor_property("static_mesh")
        if not mesh:
            continue
        mesh_path = mesh.get_path_name()
        if "/LevelPrototyping/" not in mesh_path and not mesh_path.startswith("/Engine/BasicShapes/"):
            continue
        references.append({
            "label": actor.get_actor_label(),
            "class": actor.get_class().get_name(),
            "mesh": mesh_path,
            "location": vector(actor.get_actor_location()),
            "scale": vector(actor.get_actor_scale3d()),
            "collision": str(component.get_collision_enabled()),
            "visible": bool(component.is_visible()),
            "hidden_in_game": bool(component.get_editor_property("hidden_in_game")),
            "override_materials": [
                material.get_path_name() if material else None
                for material in component.get_editor_property("override_materials")
            ],
        })
    report["maps"][map_path] = {
        "actor_count": len(actors.get_all_level_actors()),
        "temporary_mesh_references": sorted(references, key=lambda row: row["label"]),
    }

(OUT / "audit.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("PG_TEMPORARY_MODEL_AUDIT_COMPLETE " + str(OUT / "audit.json"))

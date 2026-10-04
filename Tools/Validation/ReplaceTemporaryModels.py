"""Import Blender utility meshes and replace remaining primitive map meshes.

Default mode imports and validates the authored meshes. Add
``-PGApplyTemporaryModels`` to update the five development maps. After the
native editor is rebuilt, use ``-PGValidateTemporaryModels`` to verify both
saved map references and the APGLootDrop native default.
"""

import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / "Tools" / "Art" / "UtilityModels"
OUT = ROOT / "Saved" / "TemporaryModels"
DEST = "/Game/Art/UtilityModels"
OUT.mkdir(parents=True, exist_ok=True)

MAPS = [
    "/Game/Maps/DummyDevMap",
    "/Game/Maps/EmptyDevMap",
    "/Game/Maps/EnemyDevMap",
    "/Game/Maps/FeatureDevMap",
    "/Game/Maps/StageDevMap",
]

PROTOTYPE_CUBE = "/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_Cube.SM_Cube"
ENGINE_CUBE = "/Engine/BasicShapes/Cube.Cube"
REPLACEMENTS = {
    PROTOTYPE_CUBE: "SM_PG_DevBlock_Corner",
    ENGINE_CUBE: "SM_PG_DevBlock_Centered",
}

manifest = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
command_line = unreal.SystemLibrary.get_command_line()
apply_maps = "-PGApplyTemporaryModels" in command_line
validate_only = "-PGValidateTemporaryModels" in command_line


def vector(value):
    return [round(value.x, 5), round(value.y, 5), round(value.z, 5)]


def make_material(name, definition):
    path = f"{DEST}/{name}"
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    library = unreal.MaterialEditingLibrary
    library.delete_all_material_expressions(material)

    color = library.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(*definition["color"], 1.0))
    library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    for property_name, value, y in [
        (unreal.MaterialProperty.MP_METALLIC, definition["metallic"], 180),
        (unreal.MaterialProperty.MP_ROUGHNESS, definition["roughness"], 280),
    ]:
        scalar = library.create_material_expression(material, unreal.MaterialExpressionConstant, -400, y)
        scalar.set_editor_property("r", value)
        library.connect_material_property(scalar, "", property_name)

    if definition["emission"]:
        glow = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -100, 380)
        glow.set_editor_property("const_b", definition["emission"])
        library.connect_material_expressions(color, "", glow, "A")
        library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    library.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def import_meshes():
    materials = {
        name: make_material(name, definition)
        for name, definition in manifest["materials"].items()
    }
    tasks = []
    for name, definition in manifest["meshes"].items():
        task = unreal.AssetImportTask()
        for key, value in [
            ("filename", str(SOURCE / definition["file"])),
            ("destination_path", DEST),
            ("destination_name", name),
            ("automated", True),
            ("replace_existing", True),
            ("save", True),
            ("factory", unreal.FbxFactory()),
        ]:
            task.set_editor_property(key, value)
        options = unreal.FbxImportUI()
        for key, value in [
            ("automated_import_should_detect_type", False),
            ("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH),
            ("import_mesh", True),
            ("import_materials", False),
            ("import_textures", False),
            ("import_animations", False),
        ]:
            options.set_editor_property(key, value)
        static = options.get_editor_property("static_mesh_import_data")
        for key, value in [
            ("combine_meshes", True),
            ("auto_generate_collision", False),
            ("generate_lightmap_u_vs", False),
            ("convert_scene", True),
            ("convert_scene_unit", True),
            ("force_front_x_axis", False),
            ("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS),
        ]:
            static.set_editor_property(key, value)
        task.set_editor_property("options", options)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    prototype_cube = unreal.load_asset(PROTOTYPE_CUBE.split(".")[0])
    engine_cube = unreal.load_asset(ENGINE_CUBE.split(".")[0])
    assert isinstance(prototype_cube, unreal.StaticMesh)
    assert isinstance(engine_cube, unreal.StaticMesh)
    collision_sources = {
        "SM_PG_DevBlock_Corner": prototype_cube,
        "SM_PG_DevBlock_Centered": engine_cube,
    }
    for name, definition in manifest["meshes"].items():
        mesh = unreal.load_asset(f"{DEST}/{name}")
        assert isinstance(mesh, unreal.StaticMesh), name
        mesh.set_material(0, materials[definition["material"]])
        if name in collision_sources:
            source_body = collision_sources[name].get_editor_property("body_setup")
            body = mesh.get_editor_property("body_setup")
            body.set_editor_property("agg_geom", source_body.get_editor_property("agg_geom"))
            body.set_editor_property("collision_trace_flag", source_body.get_editor_property("collision_trace_flag"))
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def validate_meshes():
    result = {}
    for name, definition in manifest["meshes"].items():
        mesh = unreal.load_asset(f"{DEST}/{name}")
        assert isinstance(mesh, unreal.StaticMesh), name
        bounds = mesh.get_bounding_box()
        actual_bounds = [vector(bounds.min), vector(bounds.max)]
        expected_bounds = [[round(axis * 100.0, 5) for axis in side] for side in definition["bounds_m"]]
        assert all(
            abs(actual_bounds[side][axis] - expected_bounds[side][axis]) < 0.03
            for side in range(2)
            for axis in range(3)
        ), (name, actual_bounds, expected_bounds)
        assert mesh.get_num_triangles(0) == definition["triangles"], name
        assert mesh.get_material(0).get_name() == definition["material"], name
        result[name] = {
            "path": mesh.get_path_name(),
            "bounds_cm": actual_bounds,
            "triangles": mesh.get_num_triangles(0),
            "material": mesh.get_material(0).get_path_name(),
        }
    corner_body = unreal.load_asset(f"{DEST}/SM_PG_DevBlock_Corner").get_editor_property("body_setup")
    centered_body = unreal.load_asset(f"{DEST}/SM_PG_DevBlock_Centered").get_editor_property("body_setup")
    assert corner_body.get_editor_property("agg_geom").export_text() == unreal.load_asset(
        PROTOTYPE_CUBE.split(".")[0]
    ).get_editor_property("body_setup").get_editor_property("agg_geom").export_text()
    assert centered_body.get_editor_property("agg_geom").export_text() == unreal.load_asset(
        ENGINE_CUBE.split(".")[0]
    ).get_editor_property("body_setup").get_editor_property("agg_geom").export_text()
    return result


def backup_maps():
    backup = ROOT / "Saved" / "Backups" / "TemporaryModels" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    for map_path in MAPS:
        name = map_path.rsplit("/", 1)[1]
        for relative in [
            Path("Maps") / f"{name}.umap",
            Path("__ExternalActors__") / "Maps" / name,
            Path("__ExternalObjects__") / "Maps" / name,
        ]:
            source = ROOT / "Content" / relative
            target = backup / relative
            if source.is_dir():
                shutil.copytree(source, target)
            elif source.is_file():
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
    return str(backup)


def replace_map_references():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    meshes = {source: unreal.load_asset(f"{DEST}/{name}") for source, name in REPLACEMENTS.items()}
    changes = []
    for map_path in MAPS:
        assert level.load_level(map_path), map_path
        map_changes = []
        for actor in actor_subsystem.get_all_level_actors():
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if not component:
                continue
            current = component.get_editor_property("static_mesh")
            current_path = current.get_path_name() if current else None
            if current_path not in meshes:
                continue
            before_transform = str(actor.get_actor_transform())
            before_collision = str(component.get_collision_enabled())
            actor.modify()
            component.modify()
            component.set_static_mesh(meshes[current_path])
            component.set_editor_property("override_materials", [])
            assert str(actor.get_actor_transform()) == before_transform
            assert str(component.get_collision_enabled()) == before_collision
            map_changes.append({
                "label": actor.get_actor_label(),
                "before": current_path,
                "after": meshes[current_path].get_path_name(),
                "transform": before_transform,
                "collision": before_collision,
            })
        assert map_changes, (map_path, "No temporary meshes found; use validation mode for an already-converted map")
        assert level.save_current_level(), map_path
        changes.append({"map": map_path, "count": len(map_changes), "actors": map_changes})
    assert sum(row["count"] for row in changes) == 65, changes
    return changes


def validate_maps():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    result = []
    replacement_paths = {f"{DEST}/{name}.{name}" for name in REPLACEMENTS.values()}
    for map_path in MAPS:
        assert level.load_level(map_path), map_path
        temporary = []
        replacements = []
        for actor in actor_subsystem.get_all_level_actors():
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh = component.get_editor_property("static_mesh") if component else None
            path = mesh.get_path_name() if mesh else None
            if path in REPLACEMENTS:
                temporary.append((actor.get_actor_label(), path))
            if path in replacement_paths:
                replacements.append((actor.get_actor_label(), path))
        assert not temporary, (map_path, temporary)
        result.append({"map": map_path, "replacement_count": len(replacements)})
    assert sum(row["replacement_count"] for row in result) == 65, result
    return result


report = {"status": "PASS", "mode": "validate" if validate_only else "apply" if apply_maps else "import"}
if not validate_only:
    import_meshes()
report["meshes"] = validate_meshes()
if apply_maps:
    report["backup"] = backup_maps()
    report["changes"] = replace_map_references()
if validate_only:
    report["maps"] = validate_maps()
    loot = unreal.get_default_object(unreal.PGLootDrop)
    beam_component = next(
        component
        for component in loot.get_components_by_class(unreal.StaticMeshComponent)
        if component.get_name() == "LootBeam"
    )
    beam_mesh = beam_component.get_editor_property("static_mesh")
    report["loot_beam"] = beam_mesh.get_path_name() if beam_mesh else None
    assert report["loot_beam"] == f"{DEST}/SM_PG_LootBeam.SM_PG_LootBeam", report["loot_beam"]

filename = "validation.json" if validate_only else "replacement.json" if apply_maps else "import.json"
(OUT / filename).write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("PG_TEMPORARY_MODEL_REPLACEMENT_COMPLETE " + json.dumps(report))

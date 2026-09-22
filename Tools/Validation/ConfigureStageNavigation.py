"""Restore runtime navigation for duplicated stage maps; preserve a map backup."""
import os, shutil, unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for name in ['RogueArena', 'StageDevMap']:
    path = '/Game/Maps/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path): continue
    source = os.path.join(unreal.Paths.project_content_dir(), 'Maps', name + '.umap')
    backup = os.path.join(unreal.Paths.project_saved_dir(), 'Backups/CombatControls', name + '.umap')
    os.makedirs(os.path.dirname(backup), exist_ok=True)
    if not os.path.exists(backup): shutil.copy2(source, backup)
    assert level.load_level(path)
    navmeshes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.RecastNavMesh)]
    assert navmeshes, 'Missing RecastNavMesh in ' + path
    for nav in navmeshes:
        nav.set_editor_property('runtime_generation', unreal.RuntimeGenerationType.DYNAMIC)
        nav.set_editor_property('force_rebuild_on_load', True)
    assert level.save_current_level()
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log('PGControls navigation configured ' + path)

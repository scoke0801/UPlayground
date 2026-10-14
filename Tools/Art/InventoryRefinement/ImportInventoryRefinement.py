"""Import the generated inventory frame without replacing the previous artwork."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
name = 'T_PGForgePanel'
task = unreal.AssetImportTask()
for key, value in dict(filename=str(root / 'Tools/Art/InventoryRefinement' / (name + '.png')),
                       destination_path='/Game/UI/Inventory', destination_name=name,
                       automated=True, replace_existing=True, save=False).items():
    task.set_editor_property(key, value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/UI/Inventory/' + name)
assert isinstance(texture, unreal.Texture2D)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
texture.set_editor_property('compression_no_alpha', False)
texture.set_editor_property('never_stream', True)
assert (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) == (256, 256)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
unreal.log('PGInventoryRefinement IMPORT PASS')

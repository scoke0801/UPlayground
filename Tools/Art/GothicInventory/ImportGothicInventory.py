"""Import the shared ImageGen source at panel-specific nine-slice resolutions."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
for name, size in [('T_PGGothicPanel', 256), ('T_PGGothicStats', 512)]:
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(root / 'Tools/Art/GothicInventory/T_PGGothicPanel.png'),
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
    # Slate nine-slice corners use resource dimensions, not FSlateBrush.ImageSize.
    texture.set_editor_property('max_texture_size', size)
    assert texture.blueprint_get_size_x() == texture.blueprint_get_size_y()
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
unreal.log('PGGothicInventory IMPORT PASS')

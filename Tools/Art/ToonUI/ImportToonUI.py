"""Import ImageGen nine-slice UI textures; run with Unreal Python commandlet."""
from pathlib import Path
import json
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
assets = []
for name, folder, size in [('T_PGToonPanel', 'Inventory', 256), ('T_PGToonPlate', 'Combat', 128)]:
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(root / 'Tools/Art/ToonUI' / (name + '.png')),
                           destination_path='/Game/UI/' + folder, destination_name=name,
                           automated=True, replace_existing=True, save=False).items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset('/Game/UI/' + folder + '/' + name)
    assert isinstance(texture, unreal.Texture2D), name
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('compression_no_alpha', False)
    texture.set_editor_property('never_stream', True)
    assert (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) == (size, size), name
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False), name
    assets.append(texture.get_path_name())
out = root / 'Saved/ToonUI'
out.mkdir(parents=True, exist_ok=True)
(out / 'import.json').write_text(json.dumps(dict(status='PASS', assets=assets), indent=2), encoding='utf-8')
unreal.log('PGToonUI IMPORT PASS')

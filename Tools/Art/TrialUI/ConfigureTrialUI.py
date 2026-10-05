"""Import the generated trial frame; run with Unreal's Python commandlet."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
destination = '/Game/DataCenter/UI/Moonlit'
path = destination + '/T_TrialFrame'
task = unreal.AssetImportTask()
task.set_editor_property('filename', str(root / 'Tools/Art/TrialUI/T_TrialFrame.png'))
task.set_editor_property('destination_path', destination)
task.set_editor_property('destination_name', 'T_TrialFrame')
task.set_editor_property('automated', True)
task.set_editor_property('replace_existing', False)
task.set_editor_property('save', False)
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset(path)
assert isinstance(texture, unreal.Texture2D), path
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
assert (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) == (1536, 1024)
out = root / 'Saved/TrialUI'
out.mkdir(parents=True, exist_ok=True)
(out / 'import.json').write_text(json.dumps(dict(status='PASS', texture=path)), encoding='utf-8')
unreal.log('TrialUI IMPORT PASS ' + path)

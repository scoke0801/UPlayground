"""Import the generated HUD plaque and transparent menu border in Unreal Python."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
destination = '/Game/DataCenter/UI/Moonlit'
sizes = {'T_HUDPlaque': (2172,724), 'T_MenuFrame': (1536,1024)}
for name, size in sizes.items():
    path = destination + '/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        for key, value in dict(filename=str(root/'Tools/Art/TrialUI'/(name+'.png')),
                               destination_path=destination, destination_name=name,
                               automated=True, replace_existing=False, save=False).items():
            task.set_editor_property(key,value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(path)
    assert isinstance(texture,unreal.Texture2D), path
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('compression_no_alpha',False)
    texture.set_editor_property('srgb',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False)
    assert (texture.blueprint_get_size_x(),texture.blueprint_get_size_y())==size
out = root/'Saved/HUDPolish'
out.mkdir(parents=True,exist_ok=True)
(out/'import.json').write_text(json.dumps(dict(status='PASS',textures=list(sizes))),encoding='utf-8')
unreal.log('HUDFrames IMPORT PASS')

"""Import original combat HUD art; execute through Unreal Python commandlet."""
from pathlib import Path
import json
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
sources={'T_PGCombatOrbFrame':'T_PGCombatOrbFrame_Anime.png',
         'T_PGCombatPlate':'T_PGCombatPlate.png','T_PGCombatPotion':'T_PGCombatPotion.png'}
assets=[]
for name,source in sources.items():
    asset='/Game/UI/Combat/'+name
    task=unreal.AssetImportTask()
    for key,value in dict(filename=str(root/'Tools/Art/CombatHUD'/source),
                          destination_path='/Game/UI/Combat',destination_name=name,
                          automated=True,replace_existing=True,save=False).items():
        task.set_editor_property(key,value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.load_asset(asset)
    assert isinstance(texture,unreal.Texture2D)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('compression_no_alpha',False)
    texture.set_editor_property('srgb',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False)
    assets.append(dict(asset=asset,source=source,size=[texture.blueprint_get_size_x(),texture.blueprint_get_size_y()]))
out=root/'Saved/CombatHUD'
out.mkdir(parents=True,exist_ok=True)
(out/'import.json').write_text(json.dumps(dict(status='PASS',style='anime fantasy',assets=assets)),encoding='utf-8')
unreal.log('PGCombatHUD IMPORT PASS')

"""Import the original ImageGen HUD plate. Safe to run again in UE 5.8."""
import os
import unreal

task = unreal.AssetImportTask()
task.filename = os.path.join(unreal.Paths.project_dir(), 'Tools/Art/MainUI/T_MainHUDPlate.png')
task.destination_path = '/Game/UI/Main'
task.destination_name = 'T_MainHUDPlate'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/UI/Main/T_MainHUDPlate')
assert texture, 'HUD plate import failed'
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
unreal.log('PGMainUI: imported and saved HUD plate')

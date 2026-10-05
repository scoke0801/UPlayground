"""Import selected illustrations and wire playable character portraits. Run in UE Python."""
import json
import sys
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterCatalog import PLAYER_IDS, portrait_source
DEST='/Game/DataCenter/UI/Moonlit'
OUT=ROOT/'Saved/MoonlitUI'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
OUT.mkdir(parents=True)
EAL=unreal.EditorAssetLibrary
NAMES=PLAYER_IDS

def backup(asset_path):
    relative=Path(asset_path.removeprefix('/Game/').split('.')[0]+'.uasset')
    source=ROOT/'Content'/relative
    if source.is_file():
        target=OUT/'backup'/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)

def run():
    imported=[]
    portraits_only='-PGPortraitsOnly' in unreal.SystemLibrary.get_command_line()
    for name in (NAMES if portraits_only else ['Sanctuary']+NAMES):
        path=DEST+'/T_'+name
        backup(path)
        task=unreal.AssetImportTask()
        task.set_editor_property('filename',str(portrait_source(ROOT,name)))
        task.set_editor_property('destination_path',DEST)
        task.set_editor_property('destination_name','T_'+name)
        task.set_editor_property('automated',True)
        task.set_editor_property('replace_existing',True)
        task.set_editor_property('save',False)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture=unreal.load_asset(path)
        assert isinstance(texture,unreal.Texture2D),path
        texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('max_texture_size',2048 if name=='Sanctuary' else 1024)
        texture.set_editor_property('srgb',True)
        if name in NAMES:
            texture.set_editor_property('compression_no_alpha',False)
        assert EAL.save_loaded_asset(texture,only_if_is_dirty=False)
        imported.append(path)
    catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    if not catalog:
        matches=[p for p in EAL.list_assets('/Game/DataCenter',recursive=True) if p.endswith('/DA_PGProgression.DA_PGProgression')]
        assert len(matches)==1,matches
        catalog=unreal.load_asset(matches[0])
    wired=[]
    for ref in catalog.get_editor_property('playable_characters'):
        asset=ref if isinstance(ref,unreal.PGCharacterAppearance) else unreal.load_asset(str(ref))
        name=str(asset.get_editor_property('id'))
        assert name in NAMES,name
        backup(asset.get_path_name())
        asset.set_editor_property('portrait',unreal.load_asset(DEST+'/T_'+name))
        assert EAL.save_loaded_asset(asset,only_if_is_dirty=False)
        wired.append(name)
    assert sorted(wired)==sorted(NAMES),wired
    report=dict(status='PASS',textures=imported,portraits=wired)
    (OUT/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log(f'MoonlitUI IMPORT PASS textures={len(imported)} portraits={len(NAMES)} '+str(OUT))

run()

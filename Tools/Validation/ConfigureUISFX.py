"""Import original UI waves and configure a dry 2D UI bus. UE Python commandlet.

Source: Tools/Art/UISFX/BuildUISFX.py. Idempotent; backs up existing packages.
Run ValidateUISFX.py in a fresh editor process after this importer.
"""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT/'Tools/Art/UISFX'
MANIFEST = json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8'))
DEST = MANIFEST['asset_directory']
BACKUP = ROOT/'Saved/Backups/UISFX'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def preserve(name):
    relative = Path(DEST.removeprefix('/Game/'))/(name+'.uasset')
    path = ROOT/'Content'/relative
    if path.exists():
        target = BACKUP/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,target)


def asset(name, cls, factory):
    preserve(name)
    result = unreal.load_asset(DEST+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name) else None
    result = result or TOOLS.create_asset(name,DEST,cls,factory())
    assert isinstance(result,cls), name
    return result


sound_class = asset('SC_PG_UI',unreal.SoundClass,unreal.SoundClassFactory)
properties = sound_class.get_editor_property('properties')
for key,value in dict(volume=MANIFEST['sound_class_volume'],is_ui_sound=True,is_music=False,
                      reverb=False,apply_ambient_volumes=False).items():
    properties.set_editor_property(key,value)
sound_class.set_editor_property('properties',properties)
submix = asset('SMX_PG_UI',unreal.SoundSubmix,unreal.SoundSubmixFactory)
assert unreal.EditorAssetLibrary.save_loaded_asset(sound_class,only_if_is_dirty=False)
assert unreal.EditorAssetLibrary.save_loaded_asset(submix,only_if_is_dirty=False)

for entry in MANIFEST['sounds']:
    path = SOURCE/entry['filename']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
    preserve(entry['name'])
    task = unreal.AssetImportTask()
    for key,value in dict(filename=str(path),destination_path=DEST,destination_name=entry['name'],
                          automated=True,replace_existing=True,save=False).items():
        task.set_editor_property(key,value)
    TOOLS.import_asset_tasks([task])
    sound = unreal.load_asset(DEST+'/'+entry['name'])
    assert isinstance(sound,unreal.SoundWave),entry['name']
    for key,value in dict(sound_class_object=sound_class,sound_submix_object=submix,
                          volume=entry['volume'],looping=False,priority=2.0,
                          loading_behavior=unreal.SoundWaveLoadingBehavior.FORCE_INLINE,
                          override_concurrency=True).items():
        sound.set_editor_property(key,value)
    sound.set_sound_asset_compression_type(unreal.SoundAssetCompressionType.PCM)
    # Per-wave concurrency: duplicate opens cannot stack, confirm never steals victory.
    concurrency = sound.get_editor_property('concurrency_overrides')
    for key,value in dict(max_count=1,resolution_rule=unreal.MaxConcurrentResolutionRule.PREVENT_NEW,
                          retrigger_time=.08,voice_steal_release_time=.025).items():
        concurrency.set_editor_property(key,value)
    sound.set_editor_property('concurrency_overrides',concurrency)
    assert unreal.EditorAssetLibrary.save_loaded_asset(sound,only_if_is_dirty=False)
    unreal.log('PGUISFX IMPORT '+entry['name'])
unreal.log('PGUISFX CONFIGURED 3 waves, UI class and submix')

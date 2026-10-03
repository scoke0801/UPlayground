"""Read-only fresh-process verification of imported UI audio and config/cook links."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root/'Tools/Art/UISFX'
manifest = json.loads((source/'manifest.json').read_text(encoding='utf-8'))
settings_class = unreal.load_class(None,'/Script/PGUI.PGUIStyleSettings')
assert settings_class
settings = unreal.get_default_object(settings_class)
slots = ('RewardOpenSound','RewardConfirmSound','VictorySound')
dest = manifest['asset_directory']
sound_class = unreal.load_asset(dest+'/SC_PG_UI')
bus = unreal.load_asset(dest+'/SMX_PG_UI')
assert sound_class and bus
properties = sound_class.get_editor_property('properties')
assert properties.get_editor_property('is_ui_sound')
assert not properties.get_editor_property('reverb')
assert not properties.get_editor_property('apply_ambient_volumes')
assert abs(properties.get_editor_property('volume')-manifest['sound_class_volume'])<.001
results=[]
for slot,entry in zip(slots,manifest['sounds']):
    assert hashlib.sha256((source/entry['filename']).read_bytes()).hexdigest()==entry['sha256']
    sound=unreal.load_asset(dest+'/'+entry['name'])
    assert isinstance(sound,unreal.SoundWave)
    assert str(settings.get_editor_property(slot))==str(sound), slot
    assert sound.get_editor_property('sound_class_object')==sound_class
    assert sound.get_editor_property('sound_submix_object')==bus
    assert sound.get_editor_property('enable_base_submix')
    assert not sound.get_editor_property('looping')
    assert sound.get_editor_property('loading_behavior')==unreal.SoundWaveLoadingBehavior.FORCE_INLINE
    assert sound.get_sound_asset_compression_type()==unreal.SoundAssetCompressionType.PCM
    assert sound.get_editor_property('num_channels')==2
    assert abs(sound.get_editor_property('duration')-entry['duration'])<.001
    assert abs(sound.get_editor_property('volume')-entry['volume'])<.001
    assert sound.get_editor_property('override_concurrency')
    concurrency=sound.get_editor_property('concurrency_overrides')
    assert concurrency.get_editor_property('max_count')==1
    assert concurrency.get_editor_property('resolution_rule')==unreal.MaxConcurrentResolutionRule.PREVENT_NEW
    results.append(dict(name=entry['name'],duration=sound.get_editor_property('duration'),status='PASS'))
assert '+DirectoriesToAlwaysCook=(Path="/Game/DataCenter")' in (root/'Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
out=root/'Saved/QA/UISFX_Assets.json'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(dict(status='PASS',assets=results,cook_directory='/Game/DataCenter'),indent=2),encoding='utf-8')
unreal.log('PGUISFX ASSETS PASS')

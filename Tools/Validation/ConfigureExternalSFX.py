"""Replace temporary PCM with supplied SFX; preserve semantic asset references.
Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
-PGSFXValidate performs a read-only fresh-process check.
"""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import shutil
import wave
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
SPEC = json.loads((ROOT/'Tools/Validation/Data/ExternalSFX.json').read_text(encoding='utf-8'))
SOURCE = ROOT/SPEC['source_directory']
DEST = '/Game/ExternalAssets/Sound/SFX'
EAL = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
VALIDATE = '-PGSFXValidate' in u.SystemLibrary.get_command_line()
OUT = ROOT/'Saved/QA/ExternalSFX'
OUT.mkdir(parents=True, exist_ok=True)
BACKUP = ROOT/'Saved/Backups/ExternalSFX'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')

def preserve(path):
    src = ROOT/'Content'/(path.removeprefix('/Game/')+'.uasset')
    if src.exists():
        dst = BACKUP/src.relative_to(ROOT/'Content')
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src,dst)

def save(asset):
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()

def import_sound(path, source):
    preserve(path)
    task = u.AssetImportTask()
    for k,v in dict(filename=str(source),destination_path=path.rsplit('/',1)[0],destination_name=path.rsplit('/',1)[1],automated=True,replace_existing=True,save=False).items():
        task.set_editor_property(k,v)
    TOOLS.import_asset_tasks([task])
    sound = u.load_asset(path)
    assert isinstance(sound,u.SoundWave),path
    sound.set_editor_property('looping',False)
    sound.set_editor_property('loading_behavior',u.SoundWaveLoadingBehavior.FORCE_INLINE)
    sound.set_sound_asset_compression_type(u.SoundAssetCompressionType.PCM)
    if '/Audio/UI/' not in path:
        sound.set_editor_property('volume',.65)
        sound.set_editor_property('override_concurrency',True)
        c=sound.get_editor_property('concurrency_overrides')
        for k,v in dict(max_count=8,retrigger_time=.025,voice_steal_release_time=.02,resolution_rule=u.MaxConcurrentResolutionRule.STOP_OLDEST).items(): c.set_editor_property(k,v)
        sound.set_editor_property('concurrency_overrides',c)
    save(sound)
    return sound

def swing_for(asset):
    return SPEC['player_swings'][str(asset.get_editor_property('skill_id'))]

registry=u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
profiles=registry.get_assets_by_class(u.TopLevelAssetPath('/Script/PGData','PGPlayerSkillProfile'),True)
checks=[]
sources=sorted(SOURCE.glob('*.wav'))
assert len(sources)==51,len(sources)
for source in sources:
    with wave.open(str(source),'rb') as wav:
        assert wav.getsampwidth() in (2,3) and wav.getnframes()>0,source
    if not VALIDATE: import_sound(DEST+'/'+source.stem,source)
    imported_sound=u.load_asset(DEST+'/'+source.stem)
    assert isinstance(imported_sound,u.SoundWave),source
    assert Path(imported_sound.get_editor_property('asset_import_data').get_first_filename()).resolve()==source.resolve()
    assert imported_sound.get_sound_asset_compression_type()==u.SoundAssetCompressionType.PCM
    with wave.open(str(source),'rb') as wav:
        assert abs(imported_sound.get_editor_property('duration')-wav.getnframes()/wav.getframerate())<.002
        assert imported_sound.get_editor_property('num_channels')==wav.getnchannels()
for path,stem in SPEC['replacements'].items():
    source=SOURCE/(stem+'.wav')
    assert source.is_file(),source
    if not VALIDATE: import_sound(path,source)
    sound=u.load_asset(path)
    assert isinstance(sound,u.SoundWave),path
    with wave.open(str(source),'rb') as wav:
        duration=wav.getnframes()/wav.getframerate()
        assert abs(sound.get_editor_property('duration')-duration)<.002,path
        assert sound.get_editor_property('num_channels')==wav.getnchannels(),path
    imported=Path(sound.get_editor_property('asset_import_data').get_first_filename()).resolve()
    assert imported==source.resolve(),(path,str(imported))
    checks.append(dict(asset=path,source=stem,duration=duration,sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
profile_checks=[]
for data in profiles:
    asset=data.get_asset()
    target=DEST+'/'+swing_for(asset)
    if not VALIDATE:
        preserve(asset.get_path_name().split('.')[0])
        asset.set_editor_property('swing_sound',u.load_asset(target));save(asset)
    assert asset.get_editor_property('swing_sound').get_path_name().split('.')[0]==target,asset.get_path_name()
    profile_checks.append(dict(profile=asset.get_path_name(),sound=target))
assert profile_checks,'No player skill profiles found'
# Check all project-authored waves, including names outside the known migration map.
authored=[]
for data in registry.get_assets_by_class(u.TopLevelAssetPath('/Script/Engine','SoundWave'),True):
    path=str(data.package_name)
    if not path.startswith('/Game/') or path.startswith('/Game/ExternalAssets/'): continue
    assert path in SPEC['replacements'], 'Unmapped project sound: '+path
    authored.append(path)
# UI import also restores the dedicated bus and authored single-voice settings.
if not VALIDATE:
    import runpy
    runpy.run_path(str(ROOT/'Tools/Validation/ConfigureUISFX.py'),run_name='__main__')
report=dict(status='PASS',mode='validate' if VALIDATE else 'apply',backup=str(BACKUP) if not VALIDATE else None,imported_sources=len(sources),replacements=checks,profiles=profile_checks,subjective_listening=False,authored_sound_inventory=authored)
(OUT/('validate.json' if VALIDATE else 'apply.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('PGExternalSFX PASS '+report['mode'])

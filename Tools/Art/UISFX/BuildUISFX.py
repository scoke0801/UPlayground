"""Build UI source metadata from supplied WAVs; never synthesize placeholder audio."""
import hashlib
import json
from pathlib import Path
import wave
ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
spec=json.loads((ROOT/'Tools/Validation/Data/ExternalSFX.json').read_text(encoding='utf-8'))
source=ROOT/spec['source_directory']
manifest=dict(schema=2,provenance='User supplied ExternalAssets/Sound/SFX',source_directory=spec['source_directory'],sound_class_volume=.8,preview_platform_headroom_db=-3.0,asset_directory='/Game/DataCenter/Audio/UI',sounds=[])
for name,volume in [('S_PG_UI_RewardOpen',.55),('S_PG_UI_RewardConfirm',.45),('S_PG_UI_Victory',.6)]:
    path=source/(spec['replacements'][manifest['asset_directory']+'/'+name]+'.wav')
    with wave.open(str(path),'rb') as wav:
        entry=dict(name=name,filename=path.name,volume=volume,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),duration=wav.getnframes()/wav.getframerate(),channels=wav.getnchannels(),sample_rate=wav.getframerate())
    manifest['sounds'].append(entry)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')

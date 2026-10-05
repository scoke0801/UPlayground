"""Back up, connect the three models, and validate source/asset reload atomically."""
import json
import os
import shutil
import sys
from datetime import datetime,timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from RunPlayableCharacterPolish import editor
from PlayableCharacterTransaction import restore,write_json

source=ROOT/'Tools/Validation/Data/PlayableCharacterPolish.json'
run=ROOT/'Saved/PlayerModels/Connections'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
run.mkdir(parents=True)
lock=ROOT/'Saved/PlayableCharacters/polish.lock'
fd=os.open(lock,os.O_CREAT|os.O_EXCL|os.O_WRONLY);os.write(fd,str(run).encode());os.close(fd)
try:
    editor(str(Path(__file__).with_name('ConnectPlayers.py')),run)
    shutil.copy2(run/'connected-source.json',source)
    editor('ValidatePlayableCharacters.py',run)
    editor('ValidatePlayableCharacterPolish.py',run)
    editor(str(ROOT/'Tools/Art/MoonlitUI/ValidateModelPortraits.py'),run)
    data=json.loads((run/'transaction.json').read_text());data['status']='VERIFIED';write_json(run/'transaction.json',data)
    report=json.loads((run/'connect.json').read_text());report['status']='PASS';write_json(run/'connect.json',report)
    print('CONNECT PASS',run,flush=True)
except BaseException:
    if (run/'transaction.json').exists():
        restore(ROOT,run);shutil.copy2(run/'polish-source.json',source)
    raise
finally:
    if not (run/'transaction.json').exists() or json.loads((run/'transaction.json').read_text())['status'] in ('VERIFIED','RESTORED'):
        lock.unlink()

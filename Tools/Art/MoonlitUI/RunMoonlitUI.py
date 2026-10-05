"""Run the UI art importer in a hidden UE 5.8 commandlet; preserve logs/backups."""
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from RunQA import run_process, read_text, FATAL
engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games/UE_5.8'
out=ROOT/'Saved/MoonlitUI'/('Import_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
out.mkdir(parents=True)
validate='--validate-model-portraits' in sys.argv
script='ValidateModelPortraits.py' if validate else 'ConfigureMoonlitUI.py'
cmd=[engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe',ROOT/'UPlayground.uproject','-EnablePlugins=PythonScriptPlugin',
     '-run=pythonscript','-script='+str(Path(__file__).with_name(script)),'-nullrhi','-unattended','-nosound','-nop4',
     '-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-abslog='+str(out/'engine.log')]
portraits_only='--portraits-only' in sys.argv
if portraits_only:
    cmd.append('-PGPortraitsOnly')
code,timeout=run_process(cmd,ROOT,out/'stdout.log',300)
log=read_text(out/'engine.log')
expected_count=7 if portraits_only else 8
marker='MoonlitUI RELOAD PASS portraits=7' if validate else f'MoonlitUI IMPORT PASS textures={expected_count} portraits=7'
ok=code==0 and not timeout and not FATAL.search(log) and marker in log
(out/'result.json').write_text(json.dumps(dict(status='PASS' if ok else 'FAIL',code=code,timeout=timeout),indent=2))
print('PASS' if ok else 'FAIL',out)
sys.exit(0 if ok else 1)

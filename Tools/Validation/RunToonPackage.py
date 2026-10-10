"""Fresh Development package containing forest gameplay and improved toon lab."""
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
from RunQA import ROOT,run_process,read_text,git_info

def main():
    engine=Path('C:/Program Files/Epic Games/UE_5.8/Engine')
    out=ROOT/'Saved/ToonImprovement'/('Package_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    out.mkdir(parents=True);print(out,flush=True)
    dotnet=next((engine/'Binaries/ThirdParty/DotNet').glob('*/win-x64/dotnet.exe'))
    command=[dotnet,engine/'Binaries/DotNET/AutomationTool/AutomationTool.dll',
        '-ScriptsForProject='+str(ROOT/'UPlayground.uproject'),'BuildCookRun','-project='+str(ROOT/'UPlayground.uproject'),
        '-noP4','-unattended','-utf8output','-platform=Win64','-clientconfig=Development','-build','-cook','-stage','-pak','-iostore',
        '-map=/Game/Maps/L_PG_ForestRuins+/Game/Maps/RogueArena+/Game/Art/ToonTest/Improvement/Maps/L_PGToon_ImprovedComparison',
        '-stagingdirectory='+str(out/'Package'),'-ddc=InstalledNoZenLocalFallback']
    (out/'manifest.json').write_text(json.dumps(dict(command=[str(c) for c in command],source=git_info(ROOT),content=git_info(ROOT/'Content')),indent=2),encoding='utf-8')
    code,timeout=run_process(command,ROOT,out/'package.log',7200)
    executables=list((out/'Package').glob('**/UPlayground/Binaries/Win64/UPlayground.exe'))
    log=read_text(out/'package.log')
    passed=not code and not timeout and 'BUILD SUCCESSFUL' in log and len(executables)==1 and 'Failed to compile Material' not in log
    report=dict(status='PASS' if passed else 'FAIL',code=code,timeout=timeout,executable=str(executables[0]) if executables else None,
        executable_sha256=hashlib.sha256(executables[0].read_bytes()).hexdigest() if executables else None)
    (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report),flush=True)
    return not passed

if __name__=='__main__':raise SystemExit(main())

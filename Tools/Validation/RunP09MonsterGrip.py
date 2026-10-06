"""Close hand views of all six saved P09 monster loadouts; no asset writes."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
import math
import os
from pathlib import Path
from RunQA import ROOT,run_process,read_text,FATAL
from MonsterVariationRoster import P09_IDS,SPEC

def expected_captures(enemy=None):
    result=set()
    for grade in SPEC['p09_grades']:
        for eid in grade['ids']:
            if enemy is not None and eid!=enemy:continue
            poses=[(0,0)]+[(sid,f) for sid in grade['skills'] for f in (.25,.45,.7)]
            for sid,fraction in poses:
                for bone in (['hand_r','hand_l'] if grade['shield'] else ['hand_r']):
                    for view in ('outer','front','palm'):
                        result.add(f'{eid}_{sid}_{round(fraction*100):02d}_{bone}_{view}.png')
    return result

def check_evidence(out,poses,expected,candidate=False):
    errors=[]
    samples=poses.get('samples',[])
    names=[f"{s['enemy']}_{s['skill']}_{round(s['fraction']*100):02d}_{s['bone']}_{s['view']}.png" for s in samples]
    if poses.get('status')!='CAPTURED':errors.append(poses.get('error','Capture did not complete'))
    if len(names)!=len(expected) or set(names)!=expected:errors.append('Pose coverage differs from the requested loadouts')
    missing=[name for name in sorted(expected) if not (out/name).is_file() or (out/name).stat().st_size==0]
    if missing:errors.append('Missing captures: '+', '.join(missing))
    if not candidate:
        if any(not math.isfinite(s.get('contact_error_cm',math.inf)) or s.get('contact_error_cm',math.inf)>=.15 for s in samples):
            errors.append('Grip contact exceeds 0.15 cm')
        if any(s.get('hand_inside_arm_bounds') is not True for s in samples):errors.append('Animated hand falls outside arm bounds')
    return errors

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--candidate',type=Path,help='Optional in-memory attachment transform overrides')
    parser.add_argument('--enemy',type=int)
    args=parser.parse_args()
    if args.enemy is not None and args.enemy not in P09_IDS:parser.error('--enemy must be one of 15201–15206')
    out=ROOT/'Saved/QA'/('P09MonsterGrip_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    out.mkdir(parents=True)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64/UnrealEditor.exe'
    command=[engine,ROOT/'UPlayground.uproject','-unattended','-nop4','-nosound','-culture=en',
        '-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-EnablePlugins=PythonScriptPlugin',
        '-UserDir='+str(out/'User'),'-PGTestProfile='+out.name,'-PGGripEvidence='+str(out),
        '-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeP09MonsterGrip.py'),
        '-RenderOffscreen','-windowed','-ResX=1280','-ResY=960','-abslog='+str(out/'engine.log')]
    if args.candidate:command.append('-PGGripCandidate='+str(args.candidate.resolve(strict=True)))
    if args.enemy:command.append('-PGGripEnemy='+str(args.enemy))
    (out/'command.json').write_text(json.dumps([str(value) for value in command],indent=2))
    evidence_files=['Tools/Validation/Data/MonsterVariations.json','Source/PGActor/Components/Rendering/PGModularAppearanceMeshComponent.cpp',
        'Source/PGActor/Components/Rendering/PGAppearanceAnimInstance.cpp','Binaries/Win64/UnrealEditor-PGActor.dll']
    (out/'inputs.json').write_text(json.dumps({path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in evidence_files},indent=2))
    print(out,flush=True)
    code,timeout=run_process(command,ROOT,out/'stdout.log',360)
    log=read_text(out/'engine.log')
    ok=not(code or timeout or FATAL.search(log) or 'LogPython: Error' in log) and 'PGP09Grip CAPTURE PASS' in log
    poses=json.loads(read_text(out/'poses.json')) if (out/'poses.json').exists() else {}
    errors=check_evidence(out,poses,expected_captures(args.enemy),bool(args.candidate))
    ok=ok and not errors
    (out/'report.json').write_text(json.dumps(dict(status='PASS' if ok else 'FAIL',code=code,timeout=timeout,visual_acceptance=False,
        candidate=bool(args.candidate),errors=errors,samples=len(poses.get('samples',[])),max_contact_error_cm=poses.get('max_contact_error_cm'),all_hands_inside_arm_bounds=poses.get('all_hands_inside_arm_bounds')),indent=2))
    print('PASS' if ok else 'FAIL',flush=True)
    return not ok

if __name__=='__main__':raise SystemExit(main())

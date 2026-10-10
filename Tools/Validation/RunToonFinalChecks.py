"""Serialize GPU validation so compiler/editor workloads do not overlap timings."""
import argparse
from datetime import datetime,timezone
import json
from pathlib import Path
import sys
from RunQA import ROOT,run_process,read_text

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-report',type=Path,required=True)
    args=parser.parse_args()
    package=json.loads(read_text(args.package_report))
    assert package['status']=='PASS'
    out=ROOT/'Saved/ToonImprovement'/('FinalChecks_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    out.mkdir(parents=True);print(out,flush=True)
    steps=[('packaged-scenes','RunToonScene.py',['--packaged-exe',package['executable']],600),
           ('controls','RunToonImprovement.py',['--step','render','--improved','--controls'],660),
           ('mobile-combat','RunMobileCombat.py',['--render-only','--configuration','Development','--characters','Bokusei','Hwarin','Hichi'],800),
           ('population','RunToonPerformance.py',['--width','1920','--height','1080','--warmup','8','--sample','12','--fixed-lod','1'],900)]
    report=dict(status='RUNNING',package=str(args.package_report.resolve()),steps=[])
    for name,script,flags,timeout in steps:
        code,timed_out=run_process([sys.executable,ROOT/'Tools/Validation'/script]+flags,ROOT,out/(name+'.log'),timeout)
        report['steps'].append(dict(name=name,status='PASS' if not code and not timed_out else 'FAIL',code=code,timeout=timed_out))
        (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(name,report['steps'][-1]['status'],flush=True)
        if code or timed_out:
            report['status']='FAIL';break
    else:report['status']='PASS'
    (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report['status']!='PASS'

if __name__=='__main__':raise SystemExit(main())

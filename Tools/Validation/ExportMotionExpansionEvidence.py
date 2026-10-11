"""Publish concise, verified adoption records; preserve the original candidate inventories."""
import argparse, csv, hashlib, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'Docs/todo/assets/MotionExpansion'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def relative(path):return path.resolve().relative_to(ROOT).as_posix()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['apply','runtime','audit','qa']:parser.add_argument('--'+name+'-run',type=Path,required=True)
    parser.add_argument('--performance-run',type=Path)
    args=parser.parse_args()
    runtime=read(args.runtime_run/'runtime.json');result=read(args.runtime_run/'runtime-result.json')
    audit=read(args.audit_run/'usage-audit.json');qa=read(args.qa_run/'report.json')
    assert runtime['status']=='PASS' and result['status']=='PASS' and result['code']==0
    assert audit['status']=='PASS' and qa['status'] in ('PASS','PASS_WITH_WARNINGS')
    assert qa['suite']=='full', 'Final evidence requires the full progression and restart regression'
    gates={gate['name']:gate['status'] for gate in qa['gates']}
    assert all(gates.get(name)=='PASS' for name in ['automation','assets','cycle','retry']), gates
    for path,digest in runtime['artifacts'].items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,('changed after runtime',path)
    manifest=read(args.apply_run/'use-manifest.json')
    verified_skills={x['skill'] for x in runtime['player_casts']+runtime['enemy_casts']}
    verified_entrances={x['enemy'] for x in runtime['entrances'] if x['observed_lock']}
    for item in manifest:
        assert item['owner'] in (verified_skills if item['kind'] in ('player','enemy') else verified_entrances)
        item['status']='saved_reference_verified' if item['kind']=='dormant' else 'runtime_binding_verified'
        item['evidence']=relative(args.apply_run/'validation.json' if item['kind']=='dormant' else args.runtime_run/'runtime.json')
    DEST.mkdir(parents=True,exist_ok=True)
    with (DEST/'motion-expansion-use-manifest.csv').open('w',encoding='utf-8-sig',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=['owner','kind','source','sequence','montage','status','evidence']);writer.writeheader();writer.writerows(manifest)
    summary=dict(status='VERIFIED_IMPLEMENTED_SCOPE',scope='P0/P1 and implemented P2; P3 weapon/high-air/leap candidates remain unadopted',
        paths={name:relative(getattr(args,name+'_run')) for name in ['apply','runtime','audit','qa']},
        candidate_count=audit['candidate_count'],adopted_bindings=len(manifest),unique_sources=len({x['source'] for x in manifest}),
        player=[{k:v for k,v in row.items() if k!='samples'} for row in runtime['player_casts']],
        enemy_skills=[x['skill'] for x in runtime['enemy_casts']],entrances=runtime['entrances'],autonomous=runtime['autonomous'],reaction=runtime['reaction'],
        automation=qa['automation_counts'],limitations=['Assisted PIE, not direct-input balance acceptance','No 20-minute packaged soak','No pre-change performance baseline','Dormant pose listed as saved reference verification'])
    summary['preserved_inventories']={}
    for name in ['art-animation-inventory.csv','dark-knight-use-manifest.csv']:
        source=DEST/name
        with source.open(encoding='utf-8-sig',newline='') as stream:
            count=sum(1 for _ in csv.DictReader(stream))
        summary['preserved_inventories'][name]=dict(rows=count,sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    if args.performance_run:
        performance=read(args.performance_run/'performance.json')
        assert performance['status']=='CAPTURED'
        assert [phase['authored_population'] for phase in performance['phases']]==[5,20,50]
        summary['performance']=performance;summary['performance_environment']=read(args.performance_run/'environment.json')
        summary['paths']['performance']=relative(args.performance_run)
        if not performance['all_frame_p95_within_60fps_budget']:
            summary['limitations'].append('Current PIE frame p95 exceeds the 60fps budget; performance acceptance is not passed')
    (DEST/'implementation-evidence.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (DEST/'usage-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=summary['status'],bindings=len(manifest)),ensure_ascii=False))
if __name__=='__main__':main()

"""Publish only successful current P09 cast, spawn and regression evidence."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
from P09WeaponRoster import SPEC, SPEC_PATH, IDS
from PlayableCharacterTransaction import write_json

ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser()
    for name in ('runtime','dungeon','qa'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    paths={name:getattr(args,name).resolve() for name in ('runtime','dungeon','qa')}
    for directory in paths.values():assert directory.is_relative_to(ROOT/'Saved/QA')
    load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
    reports={name:load(p/'report.json') for name,p in paths.items()}
    assert reports['runtime']['status']==reports['dungeon']['status']=='PASS'
    assert reports['qa']['status'] in ('PASS','PASS_WITH_WARNINGS')
    assert all(g['status']=='PASS' and g['code']==0 for name in ('runtime','dungeon') for g in reports[name]['gates'])
    runtime=load(paths['runtime']/'runtime.json');dungeon=load(paths['dungeon']/'runtime.json')
    expected={(m['id'],sid) for m in SPEC['monsters'] for sid in m['skills']}
    assert runtime['status']=='PASS' and {(c['enemy'],c['skill']) for c in runtime['casts']}==expected
    assert all(c['damage']>0 and c['hand_delta']>2 for c in runtime['casts'])
    assert {c['skill'] for c in runtime['autonomous']}=={s['id'] for s in SPEC['skills']}
    assert {m['enemy'] for m in runtime['loadouts']}==IDS
    assert dungeon['status']=='PASS' and set(dungeon['p09_weapon_ids'])==IDS and len(dungeon['runs'])==3
    inventory_root=ROOT/'Docs/todo/assets/MotionExpansion'
    preserved=load(inventory_root/'implementation-evidence.json')['preserved_inventories']
    for name,record in preserved.items():
        assert hashlib.sha256((inventory_root/name).read_bytes()).hexdigest()==record['sha256'],name
    destination=ROOT/'Docs/todo/assets/P09Weapons';destination.mkdir(parents=True,exist_ok=True)
    with (paths['runtime']/'motion-use-manifest.csv').open(encoding='utf-8-sig',newline='') as f:
        reader=csv.DictReader(f);fields=reader.fieldnames;motions=list(reader)
    for row in motions:
        if row['key'].isdigit():row['validation']='runtime_cast_and_autonomous_selection'
    with (destination/'motion-use-manifest.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader();writer.writerows(motions)
    files=list((ROOT/'Content/DataCenter/P09Weapons').rglob('*.uasset'))+[SPEC_PATH]
    files += [ROOT/'Content/DataCenter/DataTables'/p for p in ('Actor/DT_Enemy.uasset','Actor/DT_CharacterStat.uasset','Skill/DT_Skill.uasset','Path/DT_Death.uasset','Stage/DT_StageData.uasset')]
    hashes={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}
    evidence=dict(status='PASS',reports={name:str(p.relative_to(ROOT)).replace('\\','/') for name,p in paths.items()},
        monsters=SPEC['monsters'],source_motion_count=len({m['source'] for m in motions}),motion_connections=len(motions),
        casts=[{k:c[k] for k in ('enemy','skill','damage','hand_delta')} for c in runtime['casts']],autonomous=runtime['autonomous'],
        dungeon_ids=dungeon['p09_weapon_ids'],dungeon_runs=dungeon['runs'],automation=reports['qa']['automation_counts'],
        render_frames=len(list(paths['runtime'].glob('*.png'))),current_artifact_hashes=hashes,preserved_inventories=preserved,
        limitations=['직접 조작 난이도·연속 모션·청취 수용은 별도','50체 60fps 및 장시간 패키지 성능 미검증','C++ 변경 없이 기존 바이너리로 회귀 실행'])
    write_json(destination/'implementation-evidence.json',evidence)
    print(json.dumps(dict(status='PASS',monsters=len(IDS),casts=len(expected),motions=evidence['source_motion_count']),ensure_ascii=False))


if __name__=='__main__':main()

"""Saved references and authored gameplay routes; zero references never proves non-use."""
import csv
import json
import os
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))

def audit(out):
    from ConfigureMotionExpansion import SPEC, TABLES, rows
    registry=unreal.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
    options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
    with (ROOT/'Docs/todo/assets/MotionExpansion/inventory.csv').open(encoding='utf-8-sig') as stream:
        candidates=list(csv.DictReader(stream))
    result=[]
    for item in candidates:
        path=item['asset'];asset=unreal.load_asset(path)
        refs=sorted(str(p) for p in registry.get_referencers(path,options))
        result.append(dict(asset=path,loaded=bool(asset),skeleton=asset.get_editor_property('skeleton').get_path_name() if isinstance(asset,unreal.AnimSequenceBase) else None,
            registry_referencers=refs,reference_status='referenced' if refs else 'no_registry_reference_not_unused'))
    data={key:rows(path) for key,path in TABLES.items()}
    routes=[]
    for enemy in data['enemies']:
        appearances=[]
        for stage in data['stages']:
            for index,wave in enumerate(stage.get('Waves',[])):
                if any(x['MonsterId']==enemy['EnemyID'] for x in wave['MonsterSpawnInfos']):
                    appearances.append(dict(stage=stage['Id'],wave=index+1))
        routes.append(dict(enemy=enemy['EnemyID'],actor_class=enemy['ActorClass'],skills=enemy['SkillIdList'],waves=appearances,
            summoned_by=[s['SkillID'] for s in data['skills'] if s.get('SummonEnemyID')==enemy['EnemyID']]))
    assert len(result)==207 and all(x['loaded'] for x in result)
    (out/'usage-audit.json').write_text(json.dumps(dict(status='PASS',candidate_count=len(result),candidates=result,routes=routes,
        limitation='Saved hard/soft package references and table routes only. Derived source links require the adoption manifest; no unused/deletion claim.'),ensure_ascii=False,indent=2),encoding='utf-8')

if __name__=='__main__':
    audit(Path(os.environ['PG_MOTION_RUN']))
    unreal.log('PGMotionExpansion PASS')

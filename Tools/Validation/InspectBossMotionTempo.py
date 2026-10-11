"""Read saved boss contacts, montage lengths and effective source-time rates."""
import json
import os
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=Path(os.environ.get('PG_BOSS_TEMPO_RUN',str(ROOT/'Saved/BossMotionTempo')))
OUT.mkdir(parents=True,exist_ok=True)
def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/'+path)))
enemies=rows('Actor/DT_Enemy')
skills={r['SkillID']:r for r in rows('Skill/DT_Skill')}
stages=rows('Stage/DT_StageData')
bosses=[e for e in enemies if e.get('Role')=='Boss' or e['EnemyID'] in (15106,15401,15601)]
report=dict(bosses=[],stages=stages)
for enemy in bosses:
    record=dict(enemy=enemy['EnemyID'],name=enemy['EnemyName'],role=enemy.get('Role'),enemy_row=enemy,skills=[])
    for sid in sorted(set(enemy['SkillIdList']+enemy.get('PhaseTwoSkillSequence',[]))):
        row=skills[sid];item=dict(row=row,phases=[])
        path=row.get('EnemyProfile','None')
        profile=unreal.load_asset(path) if path!='None' else None
        if profile:
            item['guard']=profile.get_editor_property('bGuardCounter')
            for phase,prop in [(1,'contacts'),(2,'phase_two_contacts')]:
                contacts=list(profile.get_editor_property(prop))
                if not contacts:continue
                recovery=profile.get_editor_property('phase_two_recovery') if phase==2 else row['RecoveryDuration']
                result=[]
                for i,c in enumerate(contacts):
                    d={k:c.get_editor_property(k) for k in ('motion_start','time','start_fraction','contact_fraction','end_fraction')}
                    m=c.get_editor_property('montage');assert m
                    length=m.get_play_length()
                    end=contacts[i+1].get_editor_property('motion_start') if i+1<len(contacts) else d['time']+recovery
                    d.update(montage=m.get_path_name(),length=length,
                        windup_rate=length*(d['contact_fraction']-d['start_fraction'])/(d['time']-d['motion_start']),
                        follow_rate=length*(d['end_fraction']-d['contact_fraction'])/(end-d['time']),
                        follow_seconds=end-d['time'],blend_in=m.get_editor_property('blend_in').get_editor_property('blend_time'))
                    result.append(d)
                item['phases'].append(dict(phase=phase,contacts=result))
            if item['guard']:
                item['guard_seconds']={k:profile.get_editor_property(k) for k in ('guard_start_seconds','guard_hold_seconds','guard_accept_seconds','guard_end_seconds','counter_telegraph_seconds')}
        else:
            path=row.get('ElitePresentationMontage','None')
            m=unreal.load_asset(path) if path!='None' else None
            if m:
                length=m.get_play_length();item['length']=length
                if row['bSyncMontageToPattern']:
                    item['windup_rate']=length*(row['ImpactMontageFraction']-row['WindupMontageFraction'])/row['TelegraphDuration']
                    item['follow_rate']=length*(1-row['ImpactMontageFraction'])/max(.05,row['RecoveryDuration'])
            item['presentation_path']=path
        record['skills'].append(item)
    report['bosses'].append(record)
(OUT/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('PGBossTempo INSPECT PASS '+str(len(bosses)))

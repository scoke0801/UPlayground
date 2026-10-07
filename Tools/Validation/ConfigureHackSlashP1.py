"""Apply three opt-in profiles plus the selectable catalog, preserving source sequences.
Pose fractions for 110/114 remain candidates until continuous visual review.
"""
from datetime import datetime, timezone
import copy
import json
from pathlib import Path
import shutil
import unreal
from ConfigureAttackMotion import configure_montage_blend

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/DataCenter/HackSlashP1'
SPEC = json.loads((ROOT/'Tools/Validation/HackSlashP1.json').read_text(encoding='utf-8'))
BACKUP = ROOT/'Saved/Backups/HackSlashP1'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
BACKUP.mkdir(parents=True)
def props(obj, values):
    for key,value in values.items(): obj.set_editor_property(key,value)
    return obj
def preserve(asset):
    relative = asset.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset'
    source=ROOT/'Content'/relative; target=BACKUP/relative
    target.parent.mkdir(parents=True,exist_ok=True)
    if source.exists(): shutil.copy2(source,target)
def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)

table=unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)); before=copy.deepcopy(rows)
by_id={r['SkillID']:r for r in rows}
preserve(table); preserve(catalog)
(BACKUP/'skills.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
tools=unreal.AssetToolsHelpers.get_asset_tools(); mapping=[]
for item in SPEC['skills']:
    montage=unreal.load_asset(by_id[item['id']]['MontagePath']['AssetPath']['PackageName'])
    preserve(montage); configure_montage_blend(montage, 'UpperBody' if item.get('upper_body') else None); save(montage)
    length=montage.get_play_length()
    pose=item.get('pose_seconds')
    pose=pose+[[item['duration'],length]] if pose else [[t,f*length] for t,f in item['pose_fraction']]
    assert all(a[0]<b[0] and a[1]<b[1] for a,b in zip(pose,pose[1:])),pose
    name='DA_PlayerSkill_'+str(item['id']); profile=unreal.load_asset(DEST+'/'+name)
    if profile: preserve(profile)
    else:
        factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.PGPlayerSkillProfile)
        profile=tools.create_asset(name,DEST,unreal.PGPlayerSkillProfile,factory)
    moves=[props(unreal.PGPlayerMovementSegment(),{'segment_id':'move'+str(i),'start':start,'end':end,'distance':distance,'mode':getattr(unreal.PGPlayerMoveMode,mode),'walk_speed_ratio':item.get('walk_speed_ratio', .6),'end_cast_on_block':mode=='GROUND_LEAP'}) for i,(start,end,distance,mode) in enumerate(item['moves'])]
    phases=[]
    for i,(start,damage) in enumerate(zip(item['hits'],item['damage'])):
        policy=props(unreal.PGHitProcPolicy(),{'bleed':True,'bleed_burst':i==len(item['hits'])-1,'shock':True,'frenzy':True})
        phases.append(props(unreal.PGPlayerHitPhase(),{'phase_id':i,'start':start,'end':start+item.get('hit_window', .06 if item['id']==113 else 0),
            'shape':getattr(unreal.PGPlayerHitShape,item['shape']),'radius':item['radius'],'full_angle_degrees':item['angle'],
            'damage_multiplier':damage,'heavy_impact':item['id']!=114 and i==len(item['hits'])-1,'hit_stop_seconds':.045 if item['id']!=114 and i==len(item['hits'])-1 else .02,
            'proc_policy':policy}))
    props(profile,{'skill_id':item['id'],'duration':item['duration'],'aim_lock':item['aim'],'dodge_cancel':item['dodge'],
        'early_dodge_until':item['early_dodge'],'attack_cancel':item['attack'],'attack_speed':1.,'frenzy_per_cast_cap':3,
        'projectile_speed':1800.,'projectile_range':1000.,'projectile_lifetime':.7,'movement_segments':moves,'hit_phases':phases,
        'pose_keys':[props(unreal.PGPlayerPoseKey(),{'time':t,'montage_seconds':m}) for t,m in pose],
        'slash_material':unreal.load_asset('/Game/DataCenter/HackSlashP0/M_PlayerSlash'),'swing_sound':unreal.load_asset('/Game/DataCenter/CombatCycle/S_Normal')})
    save(profile)
    by_id[item['id']].update(PlayerProfile=profile.get_path_name(),SkillCoolTime=item['cooldown'],Desc=item['name'])
    mapping.append({'skill':item['id'],'montage':montage.get_path_name(),'length':length,'pose':pose,'continuous_review_complete':False})
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False))
save(table); catalog.set_editor_property('selectable_active_skills',[110,111,112,113,114])
catalog.set_editor_property('default_active_skills',[111,112,110,114]); save(catalog)
current=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
for old,new in zip(before,current):
    for key in old:
        if old['SkillID'] in (110,113,114) and key in ('PlayerProfile','SkillCoolTime','Desc'): continue
        assert old[key]==new[key],(old['SkillID'],key)
(BACKUP/'mapping.json').write_text(json.dumps(mapping,ensure_ascii=False,indent=2),encoding='utf-8')
marker=ROOT/'Saved/HackSlashP1_FirstBackup.txt'
if not marker.exists(): marker.write_text(str(BACKUP),encoding='utf-8')
(ROOT/'Saved/HackSlashP1_LastBackup.txt').write_text(str(BACKUP),encoding='utf-8')
unreal.log('PGHackSlashP1 APPLY PASS profiles=3 selectable=5 backup='+str(BACKUP))

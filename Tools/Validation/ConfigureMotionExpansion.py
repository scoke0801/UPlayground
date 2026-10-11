"""Project-owned derivatives only; candidate inventory is never an adoption manifest."""
import json
import copy
import csv
import os
import sys
import hashlib
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SFX_SPEC = json.loads((ROOT/'Tools/Validation/Data/ExternalSFX.json').read_text(encoding='utf-8'))
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from ConfigureMonsterVariations import TABLES, rows, ref
from PlayableCharacterTransaction import Transaction
from ConfigureAttackMotion import configure_montage_blend
OUT = Path(os.environ.get('PG_MOTION_RUN',str(ROOT/'Saved/MotionExpansionValidation')))
STEP = os.environ.get('PG_MOTION_STEP','validate')
SPEC_PATH = ROOT/'Tools/Validation/Data/MotionExpansion.json'
SPEC = json.loads(SPEC_PATH.read_text(encoding='utf-8'))
DEST = '/Game/DataCenter/MotionExpansion'
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
TX = None

def artifact_hashes():
    files=list((ROOT/'Content/DataCenter/MotionExpansion').rglob('*.uasset'))
    files += [ROOT/'Content'/(TABLES[key].removeprefix('/Game/')+'.uasset') for key in ('skills','enemies')]
    files += [SPEC_PATH]
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}

def props(obj, values):
    for key, value in values.items(): obj.set_editor_property(key, value)
    return obj

def save(asset):
    TX.mark_written(asset.get_path_name().split('.')[0])
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False)

def own(path, cls, factory):
    asset = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(path.rsplit('/',1)[1], path.rsplit('/',1)[0], cls, factory)
    assert asset, path
    return asset

def clean_clip(source, path):
    clip = unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(source, path)
    assert clip, source
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('force_root_lock', True)
    save(clip)
    return clip

def montage(path, clips):
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton', clips[0].get_editor_property('skeleton'))
    factory.set_editor_property('source_animation', clips[0])
    asset = own(path, unreal.AnimMontage, factory)
    assert unreal.PGEditorProbeTools.configure_motion_montage(asset, clips)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(asset)
    configure_montage_blend(asset,'FullBody' if '/Player/' in path else None)
    save(asset)
    return asset

def apply():
    global TX
    import ConfigureHumanoidBoss as retarget
    labels = dict(Thrust='Run_Thrust', Evade='Frank_RPG_Katana_Evade_B', GuardStart='Revenge_Guard_Start',
        GuardHold='Revenge_Guard_Loop', GuardAccept='Revenge_Guard_Accept', Counter='Revenge_Guard_Attack_ver_A', GuardEnd='Revenge_Guard_End')
    entries = {key:next(x for x in retarget.MANIFEST['motions'] if x['label']==label) for key,label in labels.items()}
    before = {key:rows(path) for key,path in TABLES.items()}
    skills = {row['SkillID']:row for row in copy.deepcopy(before['skills'])}
    enemies = {row['EnemyID']:row for row in copy.deepcopy(before['enemies'])}
    # Collisions must be checked across the full saved table before any asset writes.
    for item in SPEC['player'] + SPEC['enemy_skills']:
        assert item['id'] not in skills or skills[item['id']]['Name'] == 'MotionExpansion_'+str(item['id']), item['id']
    packages = [TABLES['skills'], TABLES['enemies'], '/Game/DataCenter/Progression/DA_PGProgression', DEST+'/Player/IK_Target', DEST+'/Player/AS_Cut']
    packages += [DEST+'/Player/'+prefix+pack for pack in retarget.SOURCE_MESHES for prefix in ('IK_', 'RTG_')]
    packages += [DEST+'/Player/AS_'+key for key in labels]
    packages += [DEST+'/Player/PGSwordSaint_'+entry['source'].rsplit('/',1)[1] for entry in entries.values()]
    packages += [DEST+'/Player/'+prefix+str(item['id']) for item in SPEC['player'] for prefix in ('AM_', 'DA_')]
    packages += [DEST+'/Enemy/AM_'+str(item['id']) for item in SPEC['enemy_skills']]
    packages += [DEST+'/Enemy/AS_'+str(item['id']) for item in SPEC['enemy_skills']]
    packages += [DEST+'/Enemy/AM_Entrance_'+str(item['enemy']) for item in SPEC['entrances']]
    packages += [DEST+'/Enemy/AS_Entrance_'+str(item['enemy'])+'_'+str(i) for item in SPEC['entrances'] for i in range(len(item['clips']))]
    packages += [DEST+'/Enemy/BS_Dormant_15502']
    TX = Transaction(ROOT, OUT); TX.prepare(packages, SPEC_PATH)
    (OUT/'before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    retarget.DEST=DEST+'/Player'; retarget.RUN=OUT; retarget.TX=TX; retarget.ENTRIES=entries
    target=unreal.load_asset('/Game/ExternalAssets/Characters/ElfSelena/BaseMesh/SK_ElfSelena'); assert target
    clips=retarget.retarget(target)
    from RetreatRootMotion import bake_retreat_root_motion
    evade_source=entries['Evade']['source']
    bake_retreat_root_motion(unreal.load_asset(evade_source),
        unreal.load_asset(DEST+'/Player/PGSwordSaint_'+evade_source.rsplit('/',1)[1]),clips['Evade'])
    save(clips['Evade'])
    cut_path=DEST+'/Player/AS_Cut'
    cut=unreal.load_asset(cut_path) if EAL.does_asset_exist(cut_path) else EAL.duplicate_asset('/Game/ExternalAssets/Animations/ElfSelena_Sword/Sword_animation/anim_attack_light_01',cut_path)
    assert cut.get_editor_property('skeleton')==target.get_editor_property('skeleton')
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(cut)
    cut.set_editor_property('enable_root_motion',False);cut.set_editor_property('force_root_lock',True);save(cut)
    manifest=[]
    for item in SPEC['player']:
        sid=item['id']; selected=[clips['Thrust']] if sid==116 else [cut,clips['Evade']] if sid==117 else [clips[k] for k in ['GuardStart','GuardHold','GuardAccept','Counter','GuardEnd']]
        am=montage(DEST+'/Player/AM_'+str(sid),selected)
        length=am.get_play_length()
        factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.PGPlayerSkillProfile)
        profile=own(DEST+'/Player/DA_'+str(sid),unreal.PGPlayerSkillProfile,factory)
        moves=[]
        if sid==116:
            poses=[[0,0],[item['hit'],length*.3],[item['duration'],length]]
            moves=[props(unreal.PGPlayerMovementSegment(),dict(segment_id='advance',start=.05,end=.28,distance=300.,mode=unreal.PGPlayerMoveMode.FORWARD_SWEEP,end_cast_on_block=False))]
        elif sid==117:
            first=cut.get_play_length()
            poses=[[0,0],[item['hit'],first*.42],[.38,first],[item['duration'],length]]
            moves=[props(unreal.PGPlayerMovementSegment(),dict(segment_id='retreat',start=.38,end=item['duration'],distance=220.,mode=unreal.PGPlayerMoveMode.BACKWARD_SWEEP,use_montage_root_motion=True,end_cast_on_block=True))]
        else:
            start=selected[0].get_play_length();hold=start+selected[1].get_play_length();accept=hold+selected[2].get_play_length()
            failure=accept+selected[3].get_play_length()
            poses=[[0,0],[.1,start],[.6,hold],[.72,accept],[item['hit'],accept+selected[3].get_play_length()*.26],[1.25,failure],[item['duration'],length]]
            props(profile,dict(guard_start=.1,guard_end=.6,guard_half_angle=65.,guard_failure_recovery=.25,guard_failure_pose_start=failure,guard_failure_pose_end=length))
        hit=props(unreal.PGPlayerHitPhase(),dict(phase_id=0,start=item['hit'],end=item['hit']+.045,radius=float(item['radius']),full_angle_degrees=float(item['angle']),damage_multiplier=item['damage'],hit_stop_seconds=.035))
        props(profile,dict(skill_id=sid,duration=item['duration'],aim_lock=.08,dodge_cancel=item['dodge'],attack_cancel=item['attack'],
            manual_counter=sid==118,swing_notify_name='None',movement_segments=moves,hit_phases=[hit],skill_description=unreal.Text(item['description']),
            pose_keys=[props(unreal.PGPlayerPoseKey(),dict(time=t,montage_seconds=m)) for t,m in poses],
            use_authored_vfx=True,default_swing_shape=unreal.PGPlayerVFXShape.THRUST if sid==116 else unreal.PGPlayerVFXShape.CRESCENT,
            slash_material=unreal.load_asset('/Game/DataCenter/HackSlashP0/M_PlayerSlash'),swing_sound=unreal.load_asset('/Game/ExternalAssets/Sound/SFX/'+SFX_SPEC['player_swings'][str(sid)])))
        save(profile)
        row=copy.deepcopy(skills[111]);row.update(Name='MotionExpansion_'+str(sid),SkillID=sid,Desc=item['name'],SkillCoolTime=item['cooldown'],MontagePath=ref(am.get_path_name()),PlayerProfile=profile.get_path_name(),ChainSkillIdList=[])
        icon='/Game/UI/SkillIcons/T_Skill_'+{116:'Dash',117:'Roll',118:'Aura'}[sid]
        row['SkillIconPath']=ref(icon)
        skills[sid]=row
        manifest += [dict(owner=sid,kind='player',source=entry['source'],sequence=clips[key].get_path_name(),montage=am.get_path_name()) for key,entry in entries.items() if clips[key] in selected]
        if sid==117:manifest.append(dict(owner=sid,kind='player',source='/Game/ExternalAssets/Animations/ElfSelena_Sword/Sword_animation/anim_attack_light_01',sequence=cut.get_path_name(),montage=am.get_path_name()))
    for item in SPEC['enemy_skills']:
        sid=item['id'];clip=clean_clip(item['source'],DEST+'/Enemy/AS_'+str(sid))
        am=montage(DEST+'/Enemy/AM_'+str(sid),[clip])
        row=copy.deepcopy(skills[item['base']]);row.update(Name='MotionExpansion_'+str(sid),SkillID=sid,Desc=item['name'],MontagePath=ref(am.get_path_name()),ElitePresentationMontage=am.get_path_name(),EnemyProfile='None',bSyncMontageToPattern=True,WindupMontageFraction=0.,ImpactMontageFraction=.45)
        row.update(item['patch']);row['SelectionTuning'].update(item['selection']);skills[sid]=row
        enemies[item['enemy']]['SkillIdList']=list(dict.fromkeys(enemies[item['enemy']]['SkillIdList']+[sid]))
        manifest.append(dict(owner=sid,kind='enemy',source=item['source'],sequence=clip.get_path_name(),montage=am.get_path_name()))
    for item in SPEC['entrances']:
        clips_in=[clean_clip(path,DEST+'/Enemy/AS_Entrance_'+str(item['enemy'])+'_'+str(i)) for i,path in enumerate(item['clips'])]
        am=montage(DEST+'/Enemy/AM_Entrance_'+str(item['enemy']),clips_in)
        enemies[item['enemy']].update(EntranceMontage=am.get_path_name(),EntranceSeconds=item['duration'],WakeDistance=item['wake'])
        for path,clip in zip(item['clips'],clips_in):manifest.append(dict(owner=item['enemy'],kind='entrance',source=path,sequence=clip.get_path_name(),montage=am.get_path_name()))
        if item.get('dormant'):
            clip=unreal.load_asset(item['dormant']);factory=unreal.BlendSpaceFactory1D();factory.set_editor_property('target_skeleton',clip.get_editor_property('skeleton'))
            blend=own(DEST+'/Enemy/BS_Dormant_15502',unreal.BlendSpace1D,factory)
            sample=props(unreal.BlendSample(),dict(animation=clip,sample_value=unreal.Vector()))
            blend.set_editor_property('sample_data',[sample]);unreal.PGHumanoidLocomotionTools.rebuild_blend_space(blend);save(blend)
            enemies[item['enemy']]['DormantLocomotion']=blend.get_path_name()
            manifest.append(dict(owner=item['enemy'],kind='dormant',source=item['dormant'],sequence=clip.get_path_name(),montage=blend.get_path_name()))
    for eid in [15501,15502,15503,15504,15302,15303,15304,15203,15204,15401]:
        enemies[eid]['SelectionReconsiderSeconds']=3.
    for eid in [15504,15203,15204]:
        enemies[eid]['Positioning'].update(bEnabled=True,ReconsiderSeconds=1.2,Separation=150.,MaxMoveDistance=240.,MinimumImprovement=45.)
    enemies[15501]['bFlightOrbit']=True
    enemies[15401].update(bPrioritizeGuard=True,GuardRepeatCooldown=14.,bReactiveGuard=True,ReactionDelay=.25,ReactionRange=500.)
    enemies[15204].update(bReactiveEvade=True,ReactionDelay=.3,ReactionRange=450.,EvadeCooldown=6.,EvadeDistance=180.,EvadeSeconds=.45,EvadeRecovery=.4)
    skills[15542]['Desc']='이동 식물 강한 물기' # Single confirmed contact, not a claimed multi-hit attack.
    for sid,preferred in [(15511,200),(15512,310),(15521,230),(15522,700),(15523,850)]:
        skills[sid]['SelectionTuning'].update(PreferredRange=preferred,RangeFalloff=700 if preferred>400 else 350,RecentUseSeconds=6.)
    for key,data in [('skills',list(skills.values())),('enemies',list(enemies.values()))]:
        table=unreal.load_asset(TABLES[key]);assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data,ensure_ascii=False));save(table)
    (OUT/'use-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush()

def validate():
    OUT.mkdir(parents=True,exist_ok=True)
    data={key:rows(path) for key,path in TABLES.items()};skills={r['SkillID']:r for r in data['skills']};enemies={r['EnemyID']:r for r in data['enemies']}
    for item in SPEC['player']:
        row=skills[item['id']];profile=unreal.load_asset(row['PlayerProfile']);am=unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
        assert profile and am and profile.get_editor_property('skill_id')==item['id']
        assert am.get_editor_property('skeleton')==unreal.load_asset('/Game/ExternalAssets/Characters/ElfSelena/BaseMesh/SKEL_ElfSelena')
        assert str(am.get_editor_property('slot_anim_tracks')[0].slot_name)=='FullBody'
        assert profile.get_editor_property('pose_keys')[-1].montage_seconds <= am.get_play_length()+.001,(item['id'],am.get_play_length())
        assert len(profile.get_editor_property('hit_phases'))==1 and row['SkillCoolTime']==item['cooldown']
        assert str(profile.get_editor_property('skill_description'))==item['description']
        assert unreal.load_asset(row['SkillIconPath']['AssetPath']['PackageName'])
        assert profile.get_editor_property('manual_counter')==(item['id']==118)
        if item['id']==117:
            move=profile.get_editor_property('movement_segments')[0]
            assert move.mode==unreal.PGPlayerMoveMode.BACKWARD_SWEEP and move.distance==220.
            from RetreatRootMotion import validate_retreat
            validate_retreat(unreal.load_asset(DEST+'/Player/AS_Evade'),profile)
    for item in SPEC['enemy_skills']:
        row=skills[item['id']];assert item['id'] in enemies[item['enemy']]['SkillIdList']
        for key,value in item['patch'].items():assert abs(row[key]-value)<.0001 if isinstance(value,(int,float)) else row[key]==value,(item['id'],key)
        for key,value in item['selection'].items():assert abs(row['SelectionTuning'][key]-value)<.0001,(item['id'],key)
        cdo=unreal.get_default_object(unreal.load_class(None,enemies[item['enemy']]['ActorClass']))
        assert unreal.load_asset(row['ElitePresentationMontage']).get_editor_property('skeleton')==cdo.mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
    catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    if STEP=='promote':assert all(x['id'] in catalog.get_editor_property('selectable_active_skills') for x in SPEC['player'])
    assert enemies[15502]['Mobility']=='Stationary' and enemies[15503]['Mobility']=='Stationary'
    assert not enemies[15502]['Positioning']['bEnabled'] and not enemies[15503]['Positioning']['bEnabled']
    for item in SPEC['entrances']:
        row=enemies[item['enemy']];assert abs(row['EntranceSeconds']-item['duration'])<.0001 and row['WakeDistance']==item['wake']
        am=unreal.load_asset(row['EntranceMontage']);assert am
        cdo=unreal.get_default_object(unreal.load_class(None,row['ActorClass']))
        assert am.get_editor_property('skeleton')==cdo.mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
    assert enemies[15401]['bReactiveGuard'] and enemies[15401]['bPrioritizeGuard'] and enemies[15401]['GuardRepeatCooldown']>=14.
    assert enemies[15204]['bReactiveEvade'] and enemies[15204]['EvadeCooldown']>=6. and enemies[15204]['EvadeRecovery']>=.39
    assert not enemies[15201]['bReactiveEvade'] and not enemies[15202]['bReactiveEvade']
    if (OUT/'before.json').exists():
        before=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
        assert data['stages']==before['stages'] and data['stats']==before['stats'] and data['deaths']==before['deaths']
        assert next(r for r in before['skills'] if r['SkillID']==115)==skills[115]
    (OUT/'validation.json').write_text(json.dumps(dict(status='PASS',player_skills=3,enemy_skills=5,entrances=4)),encoding='utf-8')

def promote():
    global TX
    evidence=Path(os.environ['PG_MOTION_EVIDENCE'])
    result=json.loads((evidence/'runtime-result.json').read_text(encoding='utf-8'))
    runtime=json.loads((evidence/'runtime.json').read_text(encoding='utf-8'))
    assert result['status']=='PASS' and result['code']==0 and runtime['status']=='PASS','runtime gate required'
    assert {x['skill'] for x in runtime['player_casts']}=={x['id'] for x in SPEC['player']}
    assert {x['skill'] for x in runtime['autonomous']}=={x['id'] for x in SPEC['enemy_skills']}
    assert len(runtime['entrances'])==len(SPEC['entrances']) and all(x['observed_lock'] for x in runtime['entrances'])
    assert runtime['reaction']['repeat_blocked'] and runtime['artifacts']==artifact_hashes(),'tested assets changed or reaction gate missing'
    path='/Game/DataCenter/Progression/DA_PGProgression'
    TX=Transaction(ROOT,OUT);TX.prepare([path],SPEC_PATH)
    catalog=unreal.load_asset(path)
    selectable=list(catalog.get_editor_property('selectable_active_skills'))
    catalog.set_editor_property('selectable_active_skills',list(dict.fromkeys(selectable+[x['id'] for x in SPEC['player']])));save(catalog)
    validate();TX.data.update(status='VERIFIED',runtime_evidence=str(evidence));TX.flush()

def inspect():
    data = {key: rows(path) for key, path in TABLES.items()}
    catalog = unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    data['catalog'] = list(catalog.get_editor_property('selectable_active_skills'))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    candidates = []
    for root in ['/Game/Art/CreatureModels', '/Game/DataCenter/HumanoidBoss', '/Game/ExternalAssets/Characters/Enemies', '/Game/Blueprints/Actor/LocalPlayer']:
        for entry in registry.get_assets_by_path(root, recursive=True):
            if str(entry.asset_class_path.asset_name) not in ('AnimSequence', 'AnimMontage', 'SkeletalMesh'): continue
            asset = entry.get_asset()
            skeleton = asset.get_editor_property('skeleton')
            candidates.append(dict(path=str(entry.package_name), skeleton=skeleton.get_path_name() if skeleton else None,
                type=str(entry.asset_class_path.asset_name), length=asset.get_play_length() if isinstance(asset, unreal.AnimSequenceBase) else None))
    data['candidates'] = candidates
    (OUT/'inspection.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')

if __name__ == '__main__':
    if STEP=='inspect':inspect()
    elif STEP=='apply':apply();validate()
    elif STEP=='validate':validate()
    elif STEP=='promote':promote()
    else:raise ValueError(STEP)
    unreal.log('PGMotionExpansion PASS')

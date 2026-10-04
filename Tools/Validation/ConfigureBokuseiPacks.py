"""Checkpointed import and direct Bokusei retarget for all authored pack clips."""
import hashlib
import json
import math
import os
import re
import sys
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
import ConfigureBokuseiMotionTest as shared
OUT=ROOT/'Saved/BokuseiPacks'
RESULTS=OUT/'Results'
RESULTS.mkdir(parents=True,exist_ok=True)
MANIFEST=json.loads((ROOT/'Tools/Art/ToonTest/Bokusei/three_packs_manifest.json').read_text(encoding='utf-8'))
SETUP=json.loads((OUT/'setup.json').read_text(encoding='utf-8'))
assert SETUP['status']=='PASS'
TARGET=unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
OPTIONS=unreal.AnimPoseEvaluationOptions()
POSE=unreal.AnimPoseExtensions
VERSION='1'
PAIRS=[('UpperArm_L','LowerArm_L'),('LowerArm_L','Hand_L'),('UpperArm_R','LowerArm_R'),('LowerArm_R','Hand_R'),
       ('UpperLeg_L','LowerLeg_L'),('LowerLeg_L','Foot_L'),('UpperLeg_R','LowerLeg_R'),('LowerLeg_R','Foot_R')]
WATCH=['Armature','Hips','Head','Hand_L','Hand_R','Foot_L','Foot_R']
REPORT=dict(status='RUNNING',clips=[],failures=[],pages=[])


def vec(v): return [v.x,v.y,v.z]


def samples(clip,bones,count=9):
    result=[]
    for i in range(count):
        pose=clip.get_anim_pose_at_time(clip.get_play_length()*i/(count-1),OPTIONS)
        result.append({b:POSE.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD) for b in bones})
    return result


def rotation_travel(frames,bone):
    first=frames[0][bone].rotation
    def delta(q):
        dot=abs(first.x*q.x+first.y*q.y+first.z*q.z+first.w*q.w)
        return math.degrees(2*math.acos(min(1,max(0,dot))))
    return max(delta(f[bone].rotation) for f in frames)


def travel(frames,bone):
    return max(math.dist(vec(f[bone].translation),vec(frames[0][bone].translation)) for f in frames)


def import_clip(entry,skeleton):
    token=entry['id'].rsplit('_',1)[-1]
    if token.isdigit() and entry['source'].endswith('_'+token+'h') and not unreal.EditorAssetLibrary.does_asset_exist(entry['source']):
        stem=entry['source'].removesuffix('_'+token+'h')
        for legacy in [stem+'_h'+token,stem+'_'+token]:
            if unreal.EditorAssetLibrary.does_asset_exist(legacy):
                assert unreal.EditorAssetLibrary.rename_asset(legacy,entry['source'])
                break
    if unreal.EditorAssetLibrary.does_asset_exist(entry['source']):
        clip=unreal.load_asset(entry['source'])
        assert clip.get_editor_property('skeleton')==skeleton
        return clip
    task=unreal.AssetImportTask()
    task.filename=entry['file']
    task.destination_path=entry['source'].rsplit('/',1)[0]
    task.destination_name=entry['name']
    task.automated=True;task.replace_existing=False;task.save=False
    task.factory=unreal.FbxFactory()
    ui=unreal.FbxImportUI()
    ui.automated_import_should_detect_type=False
    ui.import_as_skeletal=True;ui.import_mesh=False;ui.import_animations=True
    ui.import_materials=False;ui.import_textures=False;ui.create_physics_asset=False
    ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION
    ui.skeleton=skeleton
    data=ui.anim_sequence_import_data
    data.set_editor_property('use_default_sample_rate',True)
    data.set_editor_property('snap_to_closest_frame_boundary',True)
    if entry['first'] is not None and not entry.get('use_exported_range'):
        data.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_SET_RANGE)
        data.set_editor_property('frame_import_range',unreal.Int32Interval(min=round(entry['first']),max=round(entry['last'])))
    else:
        data.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.convert_scene=True;data.convert_scene_unit=True
    task.options=ui
    shared.TOOLS.import_asset_tasks([task])
    objects=[unreal.load_asset(p) for p in task.imported_object_paths]
    clips=[x for x in objects if isinstance(x,unreal.AnimSequence)]
    assert len(clips)==1,(entry['file'],task.imported_object_paths)
    clip=clips[0]
    assert clip.get_path_name().split('.')[0]==entry['source'],clip.get_path_name()
    shared.save(clip)
    return clip


def assess_source(entry,clip,setup):
    frames=samples(clip,[setup['root'],setup['pelvis']])
    root_travel=travel(frames,setup['root'])
    root_rotation=rotation_travel(frames,setup['root'])
    pelvis_travel=travel(frames,setup['pelvis'])
    mode='Body'
    if entry['group']!='InPlace':
        if root_travel>.1 or root_rotation>.1:
            mode='Root'
        elif entry['group']=='RootMotion' and pelvis_travel>1:
            mode='PelvisXY'
    return dict(mode=mode,source_root_travel_cm=root_travel,source_root_rotation_deg=root_rotation,
                source_pelvis_travel_cm=pelvis_travel,duration=clip.get_play_length())


def validate_result(entry,assessment,raw,clip,setup):
    assert isinstance(clip,unreal.AnimSequence)
    assert clip.get_editor_property('skeleton')==TARGET.get_editor_property('skeleton')
    duration=clip.get_play_length()
    assert duration>0 and abs(duration-assessment['duration'])<.001
    clip.set_editor_property('enable_root_motion',False)
    clip.set_editor_property('force_root_lock',False)
    bones=sorted(set(WATCH+[b for pair in PAIRS for b in pair]))
    frames=samples(clip,bones)
    assert all(math.isfinite(v) and abs(v)<1e6 for f in frames for t in f.values() for v in vec(t.translation))
    assert all(math.isfinite(v) for f in frames for t in f.values() for v in [t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,*vec(t.scale3d)])
    lengths=[[math.dist(vec(f[a].translation),vec(f[b].translation)) for a,b in PAIRS] for f in frames]
    variation=max(max(f[i] for f in lengths)-min(f[i] for f in lengths) for i in range(len(PAIRS)))
    assert variation<.05,(entry['label'],'Limb stretch',variation)
    probe=max(range(len(frames)),key=lambda i:max(math.dist(vec(frames[i][b].translation),vec(frames[0][b].translation)) for b in WATCH[2:]))
    row=dict(entry,**assessment,max_limb_variation_cm=variation,probe_fraction=probe/(len(frames)-1),sample_fraction=.4,
             max_bone_travel_cm={b:travel(frames,b) for b in WATCH},static_pose=max(travel(frames,b) for b in WATCH)<.01)
    if assessment['mode']!='Body':
        source_bone=setup['root'] if assessment['mode']=='Root' else setup['pelvis']
        source_frames=samples(raw,[source_bone])
        source_points=[vec(f[source_bone].translation) for f in source_frames]
        target_points=[vec(f['Armature'].translation) for f in frames]
        if assessment['mode']=='PelvisXY':
            source_points=[[p[0],p[1],0] for p in source_points]
        source_delta=[[p[j]-source_points[0][j] for j in range(3)] for p in source_points]
        target_delta=[[p[j]-target_points[0][j] for j in range(3)] for p in target_points]
        energy=sum(v*v for p in source_delta for v in p)
        if energy>.01:
            scale=sum(a*b for pa,pb in zip(source_delta,target_delta) for a,b in zip(pa,pb))/energy
            error=max(math.dist([v*scale for v in a],b) for a,b in zip(source_delta,target_delta))
            assert .05<scale<5 and error<.15,(entry['label'],'Root trajectory',scale,error)
            row.update(root_scale=scale,root_trajectory_error_cm=error)
        row['root_positions']=target_points
        clip.set_editor_property('enable_root_motion',True)
        clip.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    for key,value in {'SourceFBX':entry['relative'],'UnityClip':entry.get('unity_label',entry['label']),'RootMode':assessment['mode']}.items():
        unreal.EditorAssetLibrary.set_metadata_tag(clip,'PG.'+key,value)
    shared.save(clip)
    row.update(version=VERSION,asset_sha256=shared.digest(shared.asset_file(entry['asset'])))
    return row


def main():
    selected=MANIFEST['clips']
    if os.environ.get('PG_BOKUSEI_PACK_PILOT')=='1':
        picks={}
        for key in SETUP['sources']:
            candidates=[c for c in selected if c['rig']==key]
            for pattern in ['idle','attack','root','walk']:
                match=next((c for c in candidates if pattern in (c['label']+' '+c['relative']).lower()),None)
                if match:picks[match['id']]=match
            for c in candidates[:2]: picks[c['id']]=c
        selected=list(picks.values())
    limit=int(os.environ.get('PG_BOKUSEI_PACK_LIMIT','0'))
    if limit:selected=selected[:limit]
    for offset in range(0,len(selected),64):
        pending=[]
        for entry in selected[offset:offset+64]:
            checkpoint=RESULTS/(entry['id']+'.json')
            if checkpoint.exists():
                prior=json.loads(checkpoint.read_text(encoding='utf-8'))
                if prior['asset']!=entry['asset'] and unreal.EditorAssetLibrary.does_asset_exist(prior['asset']):
                    assert unreal.EditorAssetLibrary.rename_asset(prior['source'],entry['source'])
                    assert unreal.EditorAssetLibrary.rename_asset(prior['asset'],entry['asset'])
                    prior.update(entry)
                    prior['asset_sha256']=shared.digest(shared.asset_file(entry['asset']))
                    checkpoint.write_text(json.dumps(prior,ensure_ascii=False),encoding='utf-8')
            if checkpoint.exists() and shared.asset_file(entry['asset']).exists():
                prior=json.loads(checkpoint.read_text(encoding='utf-8'))
                if prior.get('version')==VERSION and shared.digest(shared.asset_file(entry['asset']))==prior['asset_sha256']:
                    REPORT['clips'].append(prior)
                    continue
            try:
                setup=SETUP['sources'][entry['rig']]
                mesh=unreal.load_asset(setup['mesh'])
                raw=import_clip(entry,mesh.get_editor_property('skeleton'))
                assessment=assess_source(entry,raw,setup)
                pending.append((entry,assessment))
            except Exception:
                REPORT['failures'].append(dict(id=entry['id'],file=entry['file'],error=traceback.format_exc()))
        groups={}
        for entry,assessment in pending:
            groups.setdefault((entry['rig'],entry['destination'],assessment['mode']),[]).append((entry,assessment))
        for (rig,destination,mode),entries in groups.items():
            try:
                setup=SETUP['sources'][rig]
                inputs=unreal.IKRetargetBatchOperationInputs()
                inputs.assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(e['source']) for e,_ in entries]
                inputs.source_mesh=unreal.load_asset(setup['mesh']);inputs.target_mesh=TARGET
                inputs.ik_retarget_asset=unreal.load_asset(setup['retargeters'][mode])
                inputs.target_path=destination;inputs.prefix='PGBokusei_'
                inputs.include_referenced_assets=False;inputs.overwrite_existing_files=True
                assert len(unreal.IKRetargetBatchOperation.run_batch_retarget(inputs))==len(entries)
                for entry,assessment in entries:
                    try:
                        row=validate_result(entry,assessment,unreal.load_asset(entry['source']),unreal.load_asset(entry['asset']),setup)
                        (RESULTS/(entry['id']+'.json')).write_text(json.dumps(row,ensure_ascii=False),encoding='utf-8')
                        REPORT['clips'].append(row)
                    except Exception:
                        REPORT['failures'].append(dict(id=entry['id'],file=entry['file'],error=traceback.format_exc()))
            except Exception:
                for entry,_ in entries:
                    REPORT['failures'].append(dict(id=entry['id'],file=entry['file'],error=traceback.format_exc()))
        (OUT/'progress.json').write_text(json.dumps(dict(total=len(selected),processed=min(offset+64,len(selected)),
            passed=len(REPORT['clips']),failed=len(REPORT['failures']),recent_failures=REPORT['failures'][-3:]),indent=2),encoding='utf-8')
        unreal.SystemLibrary.collect_garbage()
    categories={}
    for row in REPORT['clips']:
        categories.setdefault((row['pack'],row['category']),[]).append(row)
    for (pack,category),rows in sorted(categories.items()):
        picks={}
        for pattern in ['idle','run','attack','jump','hit']:
            candidate=next((r for r in rows if pattern in r['label'].lower()),None)
            if candidate:picks[candidate['id']]=candidate
        candidate=next((r for r in rows if r['mode']!='Body'),None)
        if candidate:picks[candidate['id']]=candidate
        for row in rows:
            if len(picks)>=6:break
            picks[row['id']]=row
        page_rows=[]
        for slot,row in enumerate(list(picks.values())[:6],1):
            label=row['label'].replace('Frank_RPG_','').replace('Frank_','').replace('M_Big_Sword@','GS_').replace('M_Katana_Blade@','KT_')
            page_rows.append(dict(row,label=str(slot)+' '+label[:25]))
        REPORT['pages'].append(dict(name=pack+'_'+category,group=pack,map='/Game/Art/ToonTest/Maps/PackLibrary/L_PGBokusei_'+pack+'_'+category,clips=page_rows))
    REPORT.update(status='PASS' if not REPORT['failures'] else 'FAIL',total=len(selected),manifest_total=len(MANIFEST['clips']))


try:
    main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT/'configure.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
if REPORT['status']!='PASS':
    raise RuntimeError('Pack conversion failed; see Saved/BokuseiPacks/configure.json')

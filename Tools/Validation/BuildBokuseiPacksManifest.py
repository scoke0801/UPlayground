"""Keep every humanoid FBX and the clip ranges authored in Unity import metadata."""
import collections
import hashlib
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def safe(value):
    return re.sub(r'[^A-Za-z0-9_]+','_',value).strip('_')


def clip_definitions(path):
    meta=Path(str(path)+'.meta')
    if not meta.exists():
        return [],None
    content=meta.read_text(encoding='utf-8')
    section=content.split('    clipAnimations:',1)[-1].split('\n    isReadable:',1)[0]
    clips=[]
    for block in re.split(r'^    - serializedVersion: \d+\s*$',section,flags=re.M)[1:]:
        values=dict(re.findall(r'^      (name|takeName|firstFrame|lastFrame|loopTime): (.*)$',block,re.M))
        if 'firstFrame' in values:
            clips.append(dict(label=values['name'].strip('"'),take=values['takeName'],first=float(values['firstFrame']),
                              last=float(values['lastFrame']),loop=values.get('loopTime')=='1'))
    return clips,hashlib.sha256(meta.read_bytes()).hexdigest()


def main():
    inventory=json.loads((ROOT/'Saved/BokuseiPacks/inventory.json').read_text())
    assert not inventory['errors'],inventory['errors'][:2]
    result=dict(clips=[],excluded=[],source_hashes={},meta_hashes={},packs={})
    for pack,info in inventory['packs'].items():
        for index,entry in enumerate(sorted(info['files'],key=lambda e:e['relative'])):
            relative=entry['relative']
            file=Path(entry['file'])
            result['source_hashes'][str(file)]=entry['sha256']
            if (pack=='FrankSlash' and not relative.startswith('Assets/Animations/')) or (pack=='GrruzamSword' and not relative.startswith('Animation/')) or (pack=='RPGAnimations' and not relative.startswith('FBX_Animations/')):
                result['excluded'].append(dict(file=str(file),reason='Model or prop; not animation library'))
                continue
            names={b[0] for b in inventory['families'][entry['signature']]['bones']}
            if not ({'pelvis','Hips','Bip01 Pelvis'} & names):
                result['excluded'].append(dict(file=str(file),reason='Weapon-only animation; no humanoid pelvis'))
                continue
            if pack=='FrankSlash':
                kind=relative.split('/')[2].removeprefix('Frank_SlashPack_')
                rig='Frank'+({'Damages':'Damage','Critical_Skills':'Damage'}.get(kind,kind))
                if kind=='Critical_Skills' and '/Skill/' in relative:
                    rig='Frank'+file.stem.rsplit('_',1)[-1]
                category=kind
            elif pack=='GrruzamSword':
                rig,category='Grruzam',relative.split('/')[1]
            else:
                rig,category='RPG',relative.split('/')[1]
            definitions,meta_hash=clip_definitions(file)
            if meta_hash:
                result['meta_hashes'][str(file)+'.meta']=meta_hash
            # No explicit Unity clip range means the FBX exported take is used.
            definitions=definitions or [dict(label=file.stem,take=None,first=None,last=None)]
            for sub_index,definition in enumerate(definitions):
                definition['unity_label']=definition['label']
                definition['label']=definition['label'].strip("\"' ")
                if re.fullmatch(r'Take[ _]?\d+',definition['label'],re.I):
                    definition['label']=file.stem
                token=hashlib.sha256((relative+'|'+str(sub_index)).encode()).hexdigest()[:10]
                if pack=='FrankSlash' and token=='65357d9b19':
                    assert entry['sha256']=='4fc185a5d8734b079a5e79ce81cf1d0d3504ada6941bb3c8776f6deb698f7a1a'
                    assert definition['first']==5940 and definition['last']==6000
                    # This particular delivered FBX contains 0..60 at 60fps,
                    # while its Unity metadata retains an earlier timeline.
                    definition['use_exported_range']=True
                    definition['range_adjustment']='Stale Unity 5940..6000; verified FBX take and key range are 0..1 seconds (0..60 at 60fps).'
                # AssetTools parses trailing digits as a numbered asset suffix,
                # even after a letter, and clamps large values to INT32_MAX.
                # End numeric identifiers with a letter to preserve all digits.
                name='AS_PG'+safe(definition['label'])[:75]+'_'+token+('h' if token.isdigit() else '')
                text=re.sub(r'[^a-z0-9]','',relative.lower())
                group='InPlace' if 'inplace' in text else 'RootMotion' if 'rootmotion' in text or '_root' in file.stem.lower() else 'Authored'
                dest='/Game/Art/ToonTest/Bokusei/Animation/'+pack+'/'+safe(category)+'/'+group
                result['clips'].append(dict(definition,pack=pack,rig=rig,category=safe(category),group=group,file=str(file),relative=relative,
                    name=name,source='/Game/Art/AnimationTests/'+pack+'/Animations/'+safe(category)+'/'+name,
                    asset=dest+'/PGBokusei_'+name,destination=dest,id=pack+'_'+token,source_sha256=entry['sha256']))
            if index%1000==0:
                print(pack,index,flush=True)
    assert len({c['asset'].lower() for c in result['clips']})==len(result['clips'])
    result['packs']=dict(collections.Counter(c['pack'] for c in result['clips']))
    path=ROOT/'Tools/Art/ToonTest/Bokusei/three_packs_manifest.json'
    path.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(result['packs'],'excluded',len(result['excluded']),'rigs',dict(collections.Counter(c['rig'] for c in result['clips'])))


if __name__=='__main__':
    main()

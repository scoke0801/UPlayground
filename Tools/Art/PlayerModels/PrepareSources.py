"""Resolve the requested Unity prefabs without changing the Unity project."""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
UNITY = Path('C:/UsingProject/UnityProject/UPlayground')
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from ToonMaterialSource import parse_material


def field(text, key):
    match = re.search(r'^  '+re.escape(key)+r': (.*)$', text, re.M)
    return match[1].strip() if match else None


def ref(text, key):
    value = field(text, key)
    return re.search(r'fileID: (-?\d+)', value)[1] if value else None


def main():
    guids={}
    for name in ('Honoka','Nenmir','Hichi'):
        for meta in (UNITY/'Assets/ExternalAssets/Character'/name).rglob('*.meta'):
            with meta.open('rb') as f: match=re.search(rb'guid: ([a-f0-9]{32})',f.read(200))
            if match: guids[match[1].decode()]=Path(str(meta)[:-5])
    # Reuse source GUIDs outside the three character packages, when available.
    extra=ROOT/'Saved/PlayerModelGuidIndex.json'
    if extra.exists(): guids.update({g:Path(p) for g,p in json.loads(extra.read_text()).items()})
    shaders={}
    for package in (UNITY/'Library/PackageCache').glob('*liltoon*'):
        for meta in package.rglob('*.shader.meta'):
            path=Path(str(meta)[:-5]); text=path.read_text(encoding='utf-8-sig')
            guid=re.search(r'guid: ([a-f0-9]{32})',meta.read_text())[1]
            name=re.search(r'Shader\s+"([^"]+)"',text)
            if name: shaders[guid]=dict(name=name[1],path=str(path))
    data=dict(destination='/Game/Art/PlayerModels', gallery_name='L_PG_PlayerModels', characters=[], source_hashes={},
        masters=dict(opaque='/Game/Art/ToonTest/Advanced/Materials/M_PGToonWorld_multi',
                     transparent='/Game/Art/ToonTest/Advanced/Materials/M_PGToonWorld_transparent'))
    def record(path): data['source_hashes'][str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
    for name in ('Hwarin','Arin','Yura'):
        prefab=UNITY/('Assets/03.Prefabs/Actor/Player/Models/PlayerModel_'+name+'.prefab')
        record(prefab)
        blocks=re.findall(r'^--- !u!(\d+) &(-?\d+)[^\n]*\n(.*?)(?=^--- !u!|\Z)',prefab.read_text(encoding='utf-8-sig'),re.M|re.S)
        gos={oid:dict(name=field(t,'m_Name').strip("'"),active=field(t,'m_IsActive')=='1') for k,oid,t in blocks if k=='1' and field(t,'m_Name')}
        ts={oid:dict(go=ref(t,'m_GameObject'),parent=ref(t,'m_Father'),text=t) for k,oid,t in blocks if k=='4' and field(t,'m_Father')}
        gt={v['go']:k for k,v in ts.items()}
        def active(go):
            trans=ts[gt[go]]
            if trans['parent']=='0': return True  # Stored model roots are disabled in the player prefab.
            return gos[go]['active'] and (active(ts[trans['parent']]['go']) if trans['parent'] in ts else True)
        renderers=[]; materials={}; sources={};excluded=[]
        for kind,oid,text in blocks:
            if kind!='137' or not field(text,'m_Mesh'): continue
            go=ref(text,'m_GameObject'); mesh=field(text,'m_Mesh')
            if not active(go) or field(text,'m_Enabled')!='1':
                excluded.append(gos[go]['name']); continue
            guid=re.search(r'guid: ([a-f0-9]{32})',mesh)[1]
            source=guids[guid];sources[guid]=str(source)
            matblock=re.search(r'^  m_Materials:\n(.*?)(?=^  \w)',text,re.M|re.S)[1]
            mats=re.findall(r'guid: ([a-f0-9]{32})',matblock)
            for g in mats:
                mat=parse_material(guids[g],guids,shaders)
                mat['depth_write']=bool(re.search(r'- _ZWrite: 1\b',guids[g].read_text(encoding='utf-8-sig')))
                if name=='Yura' and mat['name']=='deep purple':mat['profile']='hair'
                if mat['alpha_mode'] and not mat['opacity_texture']:
                    text=guids[g].read_text(encoding='utf-8-sig')
                    mask=re.search(r'- _AlphaMask:\s*\n\s*m_Texture: \{([^}]+)',text)
                    assert mask and 'fileID: 0' in mask[1],mat
                    mat['opacity_texture_default']='white'
                assert mat['shader_resolved'],mat
                assert not mat['texture_guid'] or mat['texture'],mat
                materials[g]=mat;record(guids[g])
                for key in ('texture','opacity_texture','shader_source'):
                    if mat.get(key):record(Path(mat[key]))
            weights=re.search(r'm_BlendShapeWeights:([\s\S]*?)  m_RootBone:',text)
            bones=re.search(r'  m_Bones:([\s\S]*?)  m_BlendShapeWeights:',text)
            bone_ids=re.findall(r'fileID: (-?\d+)',bones[1]) if bones else []
            renderers.append(dict(name=gos[go]['name'],source_guid=guid,materials=mats,
                weights=[float(v) for v in re.findall(r'- ([\d.eE+-]+)',weights[1])] if weights else [],
                bones=[dict(id=b,name=gos[ts[b]['go']]['name']) for b in bone_ids if b in ts],
                transform=gt[go]))
        for p in sources.values(): record(Path(p));record(Path(p+'.meta'))
        data['characters'].append(dict(name=name,prefab=str(prefab),sources=sources,renderers=renderers,
            excluded=excluded,materials=materials,bindings={}))
        print(name,len(renderers),'active renderers; excluded',excluded)
    (OUT/'sources.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')


if __name__=='__main__': main()

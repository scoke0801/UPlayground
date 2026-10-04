"""Inventory the three requested animation packs without modifying Unity files."""
import collections
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=Path('C:/UsingProject/UnityProject/UPlayground/Assets/ExternalAssets/AnimationOnly')
PACKS={'FrankSlash':'Frank_Slash_Pack','GrruzamSword':'Grruzam Powerful Sword Animation(Great Sword, Katana)',
       'RPGAnimations':'RPG_Animations_Pack'}


def inspect_fbx(path):
    with path.open('rb') as f:
        if f.read(23)!=b'Kaydara FBX Binary  \x00\x1a\x00':
            content=path.read_text(encoding='utf-8',errors='replace')
            models={(identifier or name):(name.removeprefix('Model::'),kind) for identifier,name,kind in re.findall(r'^\s*Model:\s*(?:(\d+),\s*)?"([^"]+)",\s*"([^"]+)"',content,re.M)}
            connections={a.strip('"'):b.strip('"') for a,b in re.findall(r'^\s*(?:Connect|C):\s*"OO",\s*("[^"]+"|\d+),\s*("[^"]+"|\d+)',content,re.M) if b.strip('"') in models}
            hierarchy=sorted((name,kind,models.get(connections.get(i),('',))[0]) for i,(name,kind) in models.items() if kind in ['LimbNode','Null','Root'])
            assert hierarchy,'Unrecognized ASCII FBX '+str(path)
            counts=dict(Model=len(models),Geometry=sum(kind=='Mesh' for _,kind in models.values()),
                        AnimationStack=len(re.findall(r'^\s*(?:Take|AnimationStack):',content,re.M)),
                        AnimationCurve=len(re.findall(r'^\s*(?:Channel|AnimationCurve):',content,re.M)))
            return dict(signature=hashlib.sha256(json.dumps(hierarchy).encode()).hexdigest()[:12],bones=hierarchy,counts=counts)
        version=struct.unpack('<I',f.read(4))[0]
        header='<QQQB' if version>=7500 else '<IIIB'
        size=struct.calcsize(header)
        def node():
            raw=f.read(size)
            if len(raw)!=size:
                return None
            end,count,length,n=struct.unpack(header,raw)
            if not end:
                return None
            name=f.read(n).decode('utf-8','replace')
            start=f.tell()
            props=[]
            for _ in range(count):
                kind=f.read(1)
                if kind in [b'S',b'R']:
                    length_value=struct.unpack('<I',f.read(4))[0]
                    value=f.read(length_value)
                    props.append(value.decode('utf-8','replace') if kind==b'S' else None)
                elif kind in [b'f',b'd',b'l',b'i',b'b',b'c']:
                    _,_,compressed=struct.unpack('<III',f.read(12))
                    f.seek(compressed,1)
                    props.append(None)
                else:
                    fmt={b'Y':'h',b'C':'?',b'I':'i',b'F':'f',b'D':'d',b'L':'q'}[kind]
                    props.append(struct.unpack('<'+fmt,f.read(struct.calcsize(fmt)))[0])
            return name,props,end,start+length
        models={}
        connections={}
        counts=collections.Counter()
        while True:
            top=node()
            if not top:
                break
            name,_,end,_=top
            if name in ['Objects','Connections']:
                while f.tell()<end:
                    child=node()
                    if not child:
                        break
                    kind,props,child_end,_=child
                    if name=='Objects':
                        counts[kind]+=1
                        if kind=='Model':
                            models[props[0]]=(props[1].split('\x00')[0],props[2])
                    elif kind=='C' and props[0]=='OO':
                        connections[props[1]]=props[2]
                    f.seek(child_end)
            f.seek(end)
        bones={i for i,(_,kind) in models.items() if kind in ['LimbNode','Null','Root']}
        for i in list(bones):
            parent=connections.get(i)
            while parent in models and models[parent][1]=='Null':
                bones.add(parent)
                parent=connections.get(parent)
        hierarchy=sorted((models[i][0],models[i][1],models.get(connections.get(i),('',))[0]) for i in bones)
        signature=hashlib.sha256(json.dumps(hierarchy).encode()).hexdigest()[:12]
        return dict(signature=signature,bones=hierarchy,counts=dict(counts))


def main():
    out=ROOT/'Saved/BokuseiPacks'
    out.mkdir(parents=True,exist_ok=True)
    result={'packs':{},'families':{},'errors':[]}
    for key,name in PACKS.items():
        files=sorted((SOURCE/name).rglob('*.fbx'))
        entries=[]
        for index,file in enumerate(files):
            try:
                row=inspect_fbx(file)
                family=result['families'].setdefault(row['signature'],dict(bones=row['bones'],examples=[],count=0))
                family['count']+=1
                if len(family['examples'])<4:
                    family['examples'].append(str(file))
                entries.append(dict(file=str(file),relative=file.relative_to(SOURCE/name).as_posix(),
                    sha256=hashlib.sha256(file.read_bytes()).hexdigest(),signature=row['signature'],counts=row['counts']))
            except Exception as error:
                result['errors'].append(dict(file=str(file),error=str(error)))
            if index%500==0:
                print(key,index,len(files),flush=True)
        result['packs'][key]=dict(source=str(SOURCE/name),files=entries)
    (out/'inventory.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(dict(counts={k:len(p['files']) for k,p in result['packs'].items()},
                         families={k:dict(count=v['count'],bones=len(v['bones']),example=v['examples'][0]) for k,v in result['families'].items()},
                         errors=result['errors'][:10]),indent=2))


if __name__=='__main__':
    if '--repair-errors' in sys.argv:
        path=ROOT/'Saved/BokuseiPacks/inventory.json'
        result=json.loads(path.read_text())
        errors=result['errors']
        result['errors']=[]
        for entry in errors:
            file=Path(entry['file'])
            try:
                row=inspect_fbx(file)
                key=next(k for k,v in PACKS.items() if file.is_relative_to(SOURCE/v))
                family=result['families'].setdefault(row['signature'],dict(bones=row['bones'],examples=[],count=0))
                family['count']+=1
                if len(family['examples'])<4: family['examples'].append(str(file))
                result['packs'][key]['files'].append(dict(file=str(file),relative=file.relative_to(SOURCE/PACKS[key]).as_posix(),
                    sha256=hashlib.sha256(file.read_bytes()).hexdigest(),signature=row['signature'],counts=row['counts']))
            except Exception as error:
                result['errors'].append(dict(file=str(file),error=str(error)))
        path.write_text(json.dumps(result,indent=2),encoding='utf-8')
        print({k:len(p['files']) for k,p in result['packs'].items()},'errors',len(result['errors']))
    else:
        main()

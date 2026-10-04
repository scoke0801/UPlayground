"""Read-only diagnosis of non-finite values and singular scales in Frank Whip FBX."""
import array
import json
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=Path('C:/UsingProject/UnityProject/UPlayground/Assets/ExternalAssets/AnimationOnly/Frank_Slash_Pack/Assets/Animations/Frank_SlashPack_Whip/Anim_FBX_Pack/01_Frank_Whip_Equip_All.FBX')


def main(source=SOURCE,write=True):
    models={};connections={};bad=[];zero=[];curves={}
    with source.open('rb') as f:
        assert f.read(23)==b'Kaydara FBX Binary  \x00\x1a\x00'
        version=struct.unpack('<I',f.read(4))[0]
        fmt='<QQQB' if version>=7500 else '<IIIB'
        size=struct.calcsize(fmt)
        def prop():
            kind=f.read(1)
            if kind in [b'S',b'R']:
                n=struct.unpack('<I',f.read(4))[0];value=f.read(n)
                return value.decode('utf-8','replace') if kind==b'S' else None
            if kind in [b'f',b'd',b'l',b'i',b'b',b'c']:
                count,encoding,n=struct.unpack('<III',f.read(12));data=f.read(n)
                if kind not in [b'f',b'd']:return None
                values=array.array(kind.decode())
                values.frombytes(zlib.decompress(data) if encoding else data)
                return values
            scalar={b'Y':'h',b'C':'?',b'I':'i',b'F':'f',b'D':'d',b'L':'q'}[kind]
            return struct.unpack('<'+scalar,f.read(struct.calcsize(scalar)))[0]
        def nodes(limit,owner=None):
            while f.tell()<limit:
                raw=f.read(size)
                if len(raw)!=size:return
                end,count,_,n=struct.unpack(fmt,raw)
                if not end:return
                name=f.read(n).decode();props=[prop() for _ in range(count)]
                current=owner
                if name in ['Model','AnimationCurve','AnimationCurveNode']:
                    current=props[0]
                    if name=='Model':models[current]=props[1].split('\x00')[0]
                if name=='C' and props[0] in ['OO','OP']:
                    connections.setdefault(props[1],[]).append(props[2:])
                for index,value in enumerate(props):
                    seq=value if isinstance(value,array.array) else [value]
                    invalid=[i for i,v in enumerate(seq) if isinstance(v,float) and not math.isfinite(v)]
                    if invalid:bad.append(dict(owner=current,node=name,property=index,invalid_count=len(invalid),indices=invalid[:10]))
                if name=='P' and props and props[0]=='Lcl Scaling' and any(v==0 for v in props[-3:]):
                    zero.append(dict(owner=current,node=name,scale=props[-3:]))
                if name=='KeyValueFloat' and props and isinstance(props[0],array.array):
                    values=props[0]
                    if values and any(v==0 for v in values):
                        curves[current]=dict(zero_count=sum(v==0 for v in values),count=len(values))
                nodes(end,current)
                f.seek(end)
        nodes(source.stat().st_size)
    for entry in bad+zero:
        entry['model']=models.get(entry['owner'])
        entry['targets']=[dict(bone=models.get(link[0]),property=link[1:],axis=connection[1:])
            for connection in connections.get(entry['owner'],[]) for link in connections.get(connection[0],[]) if link[0] in models]
    zero_scale_curves=[]
    for uid,row in curves.items():
        for node_connection in connections.get(uid,[]):
            for model_connection in connections.get(node_connection[0],[]):
                if len(model_connection)>1 and model_connection[1]=='Lcl Scaling':
                    zero_scale_curves.append(dict(row,bone=models.get(model_connection[0]),axis=node_connection[1:]))
    report=dict(file=str(source),nonfinite=bad,zero_reference_scales=zero,zero_scale_curves=zero_scale_curves)
    if write:
        (ROOT/'Saved/BokuseiPacks/fbx_value_diagnostic.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(json.dumps(report,indent=2))
    return report


if __name__=='__main__':
    if '--all-whip' in sys.argv:
        manifest=json.loads((ROOT/'Tools/Art/ToonTest/Bokusei/three_packs_manifest.json').read_text(encoding='utf-8'))
        reports=[main(Path(file),False) for file in sorted({c['file'] for c in manifest['clips'] if c['rig']=='FrankWhip'})]
        targets=sorted({t['bone'] for r in reports for n in r['nonfinite'] for t in n['targets']})
        report=dict(files=reports,nonfinite_bones=targets)
        (ROOT/'Saved/BokuseiPacks/whip_fbx_diagnostics.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print('Files',len(reports),'nonfinite targets',targets)
    else:
        main()

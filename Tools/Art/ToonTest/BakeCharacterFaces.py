"""Model-specific facial fields, restricted to head geometry for shared skin atlases."""
import hashlib
import importlib.util
import json
from pathlib import Path
import traceback
import numpy as np
from FaceDistanceField import distance_to, verify

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'Tools/Art/ToonTest/CharacterFaces'
BASE.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('face_baker',ROOT/'Tools/Art/ToonTest/BokuseiFaceSDF/BakeBokuseiFaceSDF.py')
baker=importlib.util.module_from_spec(spec);spec.loader.exec_module(baker)
verify()
baker.distance_to=distance_to
inventory=json.loads((ROOT/'Saved/ToonImprovement/Inventory/inventory.json').read_text(encoding='utf-8'))
slots={'Arin':0,'Hichi':5,'Honoka':1,'Hwarin':1,'LianLian':1,'Lili':2,'Nenmir':0,'P09_Female':0,'P09_Male':1,'Siuha':0}
report=dict(status='RUNNING',characters=[])
for model in inventory['characters']:
    identity=model['id']
    if identity not in slots:continue
    out=BASE/identity;out.mkdir(exist_ok=True)
    row=dict(id=identity,status='RUNNING',appearance=model['appearance'])
    report['characters'].append(row)
    try:
        slot=next(m for m in model['materials'] if m['index']==slots[identity])
        geometry=json.loads(Path(slot['geometry']).read_text(encoding='utf-8'))
        vertices=np.asarray(geometry['vertices'],dtype=float)
        tris=np.asarray(geometry['triangles'],dtype=int)
        f=np.asarray(model['head_forward']);f/=np.linalg.norm(f)
        r=np.asarray(model['head_right']);r-=f*np.dot(f,r);r/=np.linalg.norm(r)
        u=np.cross(r,f)
        local=(vertices[:,:3]-np.asarray(model['head_origin']))@np.stack([r,f,u],axis=1)
        # Skin materials can include the entire body. Head-local clipping keeps
        # chest/arm UV islands out of the facial field and leaves B=0 there.
        region=(np.abs(local[:,0])<18)&(np.abs(local[:,1])<20)&(local[:,2]>-3)&(local[:,2]<35)
        tris=tris[region[tris].all(axis=1)]
        assert len(tris)>100,(identity,'Missing head geometry',len(tris))
        used=np.unique(tris);remap=np.full(len(vertices),-1);remap[used]=np.arange(len(used))
        geometry['vertices']=vertices[used].tolist();geometry['triangles']=remap[tris].tolist()
        geo=out/'geometry.json';geo.write_text(json.dumps(geometry),encoding='utf-8')
        model_path=out/'model.json'
        source=dict(model,geometry=str(geo),geometry_sha256=hashlib.sha256(geo.read_bytes()).hexdigest())
        model_path.write_text(json.dumps(source,indent=2),encoding='utf-8')
        settings=json.loads((ROOT/'Tools/Art/ToonTest/BokuseiFaceSDF/settings.json').read_text())
        settings['bake']['minimum_coverage_texels']=1000
        settings['scalars']['FaceSDFStrength']=.7
        (out/'settings.json').write_text(json.dumps(settings,indent=2),encoding='utf-8')
        baker.main(model_path,out)
        row.update(status='PASS',slot=slot,output=str(out),bake=str(out/'bake.json'))
    except BaseException:
        row.update(status='FAIL',error=traceback.format_exc())
        print(identity,row['error'],flush=True)
    (BASE/'bakes.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
report['status']='PASS' if all(r['status']=='PASS' for r in report['characters']) else 'PARTIAL'
(BASE/'bakes.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{k:r.get(k) for k in ['id','status','error']} for r in report['characters']],ensure_ascii=False),flush=True)

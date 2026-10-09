"""Inventory owned packs and resolve standalone EastLands model/material sources."""
import hashlib
import json
from pathlib import Path
import re
import shutil

KIT=Path(__file__).resolve().parent
SOURCE=Path('C:/UsingProject/UnityProject/UPlayground/Assets/ExternalAssets/Environment')
PACK=SOURCE/'PolyartStudio/DreamscapeEastLands'
NAMES=['Osmanthus_01','Osmanthus_02','Osmanthus_Bush_01','Conifer_Twisted_01',
       'StoneArch_01','Cliff_01','BigBoulder_01','MedBoulder_02','SmallBoulder_01',
       'FlatStone_01','FlatStone_02','RiverRocks_Cluster_01','StoneWall_01','StoneWall_02',
       'StoneWall_Rock_01','StoneStairs_Rocks_02','HeroTree_ShrineFloor','SpiritStatue_01',
       'Fox_Statue','Fox_Statue_Base','Lamp_Ground_01','Log_Large','SmallGrass_02',
       'MediumGrass_02','CirclePlant_01','Iris_01','FlowerGroup_01','LargeLeaves_01']

def main():
    guid={}
    for p in PACK.rglob('*.meta'):
        m=re.search(r'^guid: (\w+)',p.read_text(encoding='utf-8',errors='replace'),re.M)
        if m:guid[m[1]]=Path(str(p)[:-5])
    report=dict(schema=1,source_root=str(PACK),meshes=[],materials={},textures={},inventory={})
    for directory in SOURCE.iterdir():
        if directory.is_dir():
            files=[p for p in directory.rglob('*.fbx') if 'animat' not in str(p).lower()]
            report['inventory'][directory.name]=dict(fbx_count=len(files),total_bytes=sum(p.stat().st_size for p in files))
    def copy(p):
        target=KIT/'DetailedSource'/p.relative_to(PACK)
        target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():shutil.copy2(p,target)
        return str(target.relative_to(KIT))
    def material(p):
        name=p.stem
        if name in report['materials']:return name
        txt=p.read_text(encoding='utf-8')
        refs={}
        for key,g in re.findall(r'- ([\w]+):\s*\n\s*m_Texture: \{fileID: \d+, guid: (\w+)',txt):
            tex=guid.get(g)
            if tex and tex.is_file():
                refs[key]=tex.stem
                report['textures'][tex.stem]=dict(file=copy(tex),sha256=hashlib.sha256(tex.read_bytes()).hexdigest())
        floats={k:float(v) for k,v in re.findall(r'^    - (\w+): (-?[\d.]+)\s*$',txt,re.M)}
        report['materials'][name]=dict(source=copy(p),textures=refs,floats=floats)
        return name
    for name in NAMES:
        p=next(PACK.rglob('SM_EastLands_'+name+'.fbx'))
        txt=Path(str(p)+'.meta').read_text(encoding='utf-8')
        mapping={}
        for slot,g in re.findall(r'name: ([^\n]+)\n\s*second: \{fileID: \d+, guid: (\w+)',txt):
            mat=guid.get(g)
            if mat and mat.suffix=='.mat':mapping[slot.strip()]=material(mat)
        if not mapping and name=='Log_Large':
            mat=next(PACK.rglob('M_EastLands_Logs.mat'))
            mapping['*']=material(mat)
        assert mapping,(name,'No material remapping')
        report['meshes'].append(dict(name=name,file=copy(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),materials=mapping))
    for name in ['T_EastLands_ForestGround_01_C','T_EastLands_ForestGround_01_N','T_EastLands_ForestGround_02_C',
                 'T_EastLands_ForestGround_02_N','T_EastLands_Moss_CR','T_EastLands_Moss_N']:
        p=next(p for p in PACK.rglob(name+'.*') if p.suffix!='.meta')
        report['textures'][name]=dict(file=copy(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest())
    (KIT/'detailed_sources.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Prepared',len(report['meshes']),'models',len(report['materials']),'materials',len(report['textures']),'textures')

if __name__=='__main__':main()

import json
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterPolish import snapshot, preflight
source=json.loads((ROOT/'Tools/Validation/Data/PlayableCharacterPolish.json').read_text(encoding='utf-8'))
preflight(source)
result={'meshes':{},'live_assets':{p:snapshot(unreal.load_asset(p)) for p in source['assets']}}
for old,new in [('Honoka','Hwarin'),('Nenmir','Arin'),('Hichi','Yura')]:
    for name,path in [(old,source['assets']['/Game/DataCenter/Characters/DA_'+old]['fields']['mesh']),
                      (new,'/Game/Art/PlayerModels/'+new+'/SK_PG_'+new)]:
        mesh=unreal.load_asset(path)
        comp=unreal.SkeletalMeshComponent();comp.set_skeletal_mesh_asset(mesh)
        rig=unreal.IKRigDefinition();ctl=unreal.IKRigController.get_controller(rig);ctl.set_skeletal_mesh(mesh)
        rows=[]
        for i in range(comp.get_num_bones()):
            bone=comp.get_bone_name(i);t=ctl.get_ref_pose_transform_of_bone(bone)
            rows.append(dict(name=str(bone),parent=str(comp.get_parent_bone(bone)),transform=t.export_text()))
        result['meshes'][name]=rows
(ROOT/'Saved/PlayerModels/connection-inspection.json').write_text(json.dumps(result,indent=2),encoding='utf-8')

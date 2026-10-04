"""Inspect source root/pelvis trajectories before root-motion conversion."""
import json
import math
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
manifest=json.loads((root/'Tools/Art/ToonTest/Bokusei/motion_library_manifest.json').read_text())
result={'status':'PASS','clips':[]}
for entry in manifest['clips']:
    if entry['group']!='Root_Motion':
        continue
    clip=unreal.load_asset(manifest['source_destination']+'/Animations/RootMotion/'+entry['file'])
    row={'name':entry['file'],'modes':{}}
    for retarget in [True,False]:
        options=unreal.AnimPoseEvaluationOptions()
        options.set_editor_property('should_retarget',retarget)
        frames=[]
        for i in range(31):
            pose=clip.get_anim_pose_at_time(clip.get_play_length()*i/30,options)
            frames.append({b:unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD) for b in ['SK_Mannequin','root','pelvis']})
        def vec(v): return [v.x,v.y,v.z]
        row['modes'][str(retarget)]={b:dict(travel_cm=max(math.dist(vec(f[b].translation),vec(frames[0][b].translation)) for f in frames),
            first=str(frames[0][b]),last=str(frames[-1][b])) for b in frames[0]}
    result['clips'].append(row)
(root/'Saved/BokuseiMotionLibrary/root_motion_probe.json').write_text(json.dumps(result,indent=2))

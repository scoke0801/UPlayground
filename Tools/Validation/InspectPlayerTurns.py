"""Read-only sampling of the available sword turn clips."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
rows = []
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for entry in registry.get_assets_by_path('/Game/Art/AnimationTests/FrankSlash/Animations/Sword2'):
    if '_Turn_' not in str(entry.package_name):
        continue
    clip = unreal.load_asset(str(entry.package_name))
    frames = []
    for i in range(21):
        pose = clip.get_anim_pose_at_time(clip.get_play_length()*i/20, unreal.AnimPoseEvaluationOptions())
        frame = {}
        for bone in ('root', 'pelvis', 'foot_l', 'foot_r'):
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            frame[bone] = dict(yaw=transform.rotation.rotator().yaw,
                               xyz=[transform.translation.x, transform.translation.y, transform.translation.z])
        frames.append(frame)
    rows.append(dict(path=clip.get_path_name(), seconds=clip.get_play_length(),
                     root_motion=clip.get_editor_property('enable_root_motion'),
                     root_lock=clip.get_editor_property('force_root_lock'), frames=frames))
out = root/'Saved/PlayerTurns'
out.mkdir(parents=True, exist_ok=True)
(out/'source-turns.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
unreal.log('PGPlayerTurns INSPECT PASS')

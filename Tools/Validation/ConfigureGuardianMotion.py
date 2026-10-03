"""Bake guardian-only shield locomotion/slam, then bind child AnimBP to enemy 15103.

Run in UE Python commandlet after ConfigureGuardianPresentation.py. Source animation
assets, shared AnimBPs and gameplay values are never edited. Authored controls live
in Tools/Art/Guardian/GuardianMotion.json; runtime needs no procedural IK or tick.
"""
import hashlib
import json
import math
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DATA = '/Game/DataCenter/Guardian'
SOURCE = '/Game/ExternalAssets/Characters/Enemies/SkeletonEnemy/Animations/'
BASE = '/Game/Blueprints/Actor/NonPlayer/Enemy/Skeleton/Anim/'
CONFIG = json.loads((ROOT/'Tools/Art/Guardian/GuardianMotion.json').read_text(encoding='utf-8'))
OUT = ROOT/'Saved/Guardian/Motion'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
BACKUP = OUT/'backup'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
POSE = unreal.AnimPoseExtensions
MATH = unreal.MathLibrary
WORLD = unreal.AnimPoseSpaces.WORLD
LOCAL = unreal.AnimPoseSpaces.LOCAL
OPTIONS = unreal.AnimPoseEvaluationOptions()
preserved = set()
source_hashes = {}


def package_file(path):
    return Path(path.split('.')[0].removeprefix('/Game/')+'.uasset')


def preserve(path):
    relative = package_file(path)
    if relative in preserved:
        return
    preserved.add(relative)
    source = ROOT/'Content'/relative
    if source.is_file():
        target = BACKUP/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def source_asset(path):
    source_hashes[path] = hashlib.sha256((ROOT/'Content'/package_file(path)).read_bytes()).hexdigest()
    asset = unreal.load_asset(path)
    assert asset, path
    return asset


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def duplicate(source, name):
    path = DATA+'/'+name
    preserve(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    return unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), path)


def vec(values):
    return unreal.Vector(*values)


def length(v):
    return math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z)


def dot(a, b):
    return a.x*b.x+a.y*b.y+a.z*b.z


def normalized(v):
    return v/max(1.e-8, length(v))


def arm(pose, side, target, pole, hand_rotation):
    """Offline two-bone IK. Rotate bones; preserve translations/scales and limb lengths."""
    upper_name, lower_name, hand_name = ('upperarm_'+side, 'lowerarm_'+side, 'hand_'+side)
    upper, lower, hand = [POSE.get_bone_pose(pose, b, WORLD) for b in (upper_name, lower_name, hand_name)]
    origin = upper.translation
    a, b = length(lower.translation-origin), length(hand.translation-lower.translation)
    direction = normalized(target-origin)
    distance = min(a+b-.05, max(abs(a-b)+.05, length(target-origin)))
    projected = pole-origin-direction*dot(pole-origin, direction)
    bend = normalized(projected)
    adjacent = (a*a-b*b+distance*distance)/(2*distance)
    elbow = origin+direction*adjacent+bend*math.sqrt(max(0, a*a-adjacent*adjacent))
    upper.rotation = MATH.quat_find_between_vectors(lower.translation-origin, elbow-origin)*upper.rotation
    pose = POSE.set_bone_pose(pose, upper, upper_name, WORLD)
    lower, hand = [POSE.get_bone_pose(pose, name, WORLD) for name in (lower_name, hand_name)]
    lower.rotation = MATH.quat_find_between_vectors(hand.translation-lower.translation, target-lower.translation)*lower.rotation
    pose = POSE.set_bone_pose(pose, lower, lower_name, WORLD)
    hand = POSE.get_bone_pose(pose, hand_name, WORLD)
    hand.rotation = hand_rotation
    return POSE.set_bone_pose(pose, hand, hand_name, WORLD)


def interpolate_key(t):
    keys = CONFIG['slam_keys']
    for a, b in zip(keys, keys[1:]):
        if t <= b['time']:
            alpha = max(0, min(1, (t-a['time'])/(b['time']-a['time'])))
            alpha = alpha*alpha*(3-2*alpha)
            return {key: (vec(a[key])*(1-alpha)+vec(b[key])*alpha if key.endswith('hand')
                          else a[key]*(1-alpha)+b[key]*alpha) for key in ['right_hand','left_hand','lean']}
    raise AssertionError(t)


idle = source_asset(SOURCE+'Anim_Idle_Sword')
enemy_table = source_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')
skill_table = source_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
skill = next(r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(skill_table)) if r['SkillID']==15103)
assert skill['bSyncMontageToPattern'] and abs(skill['ImpactMontageFraction']-CONFIG['slam_impact_fraction'])<.0001
reference = idle.get_anim_pose_at_time(0, OPTIONS)
reference_pelvis = POSE.get_bone_pose(reference, 'pelvis', WORLD).translation
hand_rotations = {side:POSE.get_bone_pose(reference, 'hand_'+side, WORLD).rotation for side in ['l','r']}


def bake(source_name, destination, slam=False):
    source = source_asset(SOURCE+source_name)
    clip = duplicate(source, destination)
    duration = source.get_play_length()
    frames = round(duration*CONFIG['sample_rate'])
    bones = [str(b) for b in unreal.AnimationLibrary.get_animation_track_names(source)]
    tracks = {b:[] for b in bones}
    for frame in range(frames+1):
        t = frame/frames
        # Grounded original lower-body motion; authored upper body is baked offline.
        pose = (idle if slam else source).get_anim_pose_at_time(t*(idle if slam else source).get_play_length(), OPTIONS)
        offset = POSE.get_bone_pose(pose, 'pelvis', WORLD).translation-reference_pelvis
        if slam:
            key = interpolate_key(t)
            spine = POSE.get_bone_pose(pose, 'spine_01', WORLD)
            spine.rotation = unreal.Rotator(pitch=0, yaw=0, roll=key['lean']).quaternion()*spine.rotation
            pose = POSE.set_bone_pose(pose, spine, 'spine_01', WORLD)
            pose = arm(pose, 'r', key['right_hand']+offset, vec([-65,-20,115])+offset, hand_rotations['r'])
            left_target = key['left_hand']+offset
        else:
            left_target = vec(CONFIG['guard_hand'])+offset
            pose = arm(pose, 'r', vec(CONFIG['slam_keys'][0]['right_hand'])+offset,
                       vec([-65,-20,115])+offset, hand_rotations['r'])
        pose = arm(pose, 'l', left_target, vec(CONFIG['guard_elbow_pole'])+offset, hand_rotations['l'])
        for bone in bones:
            tracks[bone].append(POSE.get_bone_pose(pose, bone, LOCAL))
    controller = clip.get_editor_property('controller')
    controller.open_bracket('Bake guardian shield motion', should_transact=False)
    try:
        controller.set_frame_rate(unreal.FrameRate(CONFIG['sample_rate'],1), should_transact=False)
        controller.set_number_of_frames(unreal.FrameNumber(frames), should_transact=False)
        for bone, transforms in tracks.items():
            assert controller.set_bone_track_keys(bone, [x.translation for x in transforms],
                [x.rotation for x in transforms], [x.scale3d for x in transforms], should_transact=False), bone
    finally:
        controller.close_bracket(should_transact=False)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
    save(clip)
    return clip


clips = {name:bake('Anim_'+name+'_Sword','AS_PGGuardian'+name) for name in ['Idle','Walk','Run']}
slam = bake(CONFIG['slam_source'],'AS_PGGuardianSlam',slam=True)
unreal.EditorAssetLibrary.set_metadata_tag(slam,'PGGuardianImpactFraction',str(CONFIG['slam_impact_fraction']))
save(slam)
blendspaces = {}
for name in ['Default','Strafing']:
    source = source_asset(BASE+'BS_Enemy_SkeletonWarrior_'+name)
    blend = duplicate(source,'BS_PGGuardian'+name)
    samples = list(source.get_editor_property('sample_data'))
    for sample in samples:
        source_name = sample.get_editor_property('animation').get_name()
        sample.set_editor_property('animation', clips[source_name.removeprefix('Anim_').removesuffix('_Sword')])
    blend.set_editor_property('sample_data',samples)
    save(blend)
    blendspaces[name] = blend

parent = source_asset(BASE+'ABP_Enemy_SkeletonWarrior')
path = DATA+'/ABP_PGGuardian'
preserve(path)
if unreal.EditorAssetLibrary.does_asset_exist(path):
    blueprint = unreal.load_asset(path)
else:
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property('parent_class', parent.generated_class())
    factory.set_editor_property('target_skeleton', idle.get_editor_property('skeleton'))
    blueprint = TOOLS.create_asset('ABP_PGGuardian',DATA,unreal.AnimBlueprint,factory)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
# The parent graph reads exposed CDO variables, not fixed player-node assets.
defaults = unreal.get_default_object(blueprint.generated_class())
for name, replacement in blendspaces.items():
    defaults.set_editor_property(name+'BlendSpace',replacement)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
for name, replacement in blendspaces.items():
    assert unreal.get_default_object(blueprint.generated_class()).get_editor_property(name+'BlendSpace')==replacement
save(blueprint)

montage = unreal.load_asset(DATA+'/AM_PGGuardianSlam')
preserve(montage.get_path_name())
tracks = list(montage.get_editor_property('slot_anim_tracks'))
assert len(tracks)==1
track = tracks[0].get_editor_property('anim_track')
segments = list(track.get_editor_property('anim_segments'))
assert len(segments)==1
segment = segments[0]
segment.set_editor_property('anim_reference',slam)
segment.set_editor_property('anim_start_time',0.)
segment.set_editor_property('anim_end_time',slam.get_play_length())
track.set_editor_property('anim_segments',segments)
tracks[0].set_editor_property('anim_track',track)
montage.set_editor_property('slot_anim_tracks',tracks)
unreal.AnimationLibrary.remove_all_animation_notify_tracks(montage)
save(montage)

# Recalibrate shield grip against the new guard pose. Shoulder armor follows its bones.
presentation = unreal.load_asset(DATA+'/DA_PGGuardianPresentation')
preserve(presentation.get_path_name())
pieces = list(presentation.get_editor_property('armor'))
new_pose = clips['Idle'].get_anim_pose_at_time(0,OPTIONS)
for piece in pieces:
    if str(piece.get_editor_property('name'))!='PG_GuardianShield':
        continue
    bone = piece.get_editor_property('socket')
    old_bone = POSE.get_bone_pose(reference,bone,WORLD)
    new_bone = POSE.get_bone_pose(new_pose,bone,WORLD)
    for field, state in [('guard_transform','guard'),('recovery_transform','recovery')]:
        old = piece.get_editor_property(field)
        target = MATH.compose_transforms(old,old_bone)
        target.translation = vec(CONFIG[state+'_shield_center'])
        pitch,yaw,roll = CONFIG[state+'_shield_rotation']
        target.rotation = unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll).quaternion()
        piece.set_editor_property(field,MATH.make_relative_transform(target,new_bone))
presentation.set_editor_property('armor',pieces)
save(presentation)

enemy_bp = unreal.load_asset('/Game/DataCenter/RoguelikeMVP/BP_Rogue_15103')
preserve(enemy_bp.get_path_name())
cdo = unreal.get_default_object(enemy_bp.generated_class())
cdo.get_editor_property('mesh').set_editor_property('anim_class',blueprint.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(enemy_bp)
save(enemy_bp)

for path, digest in source_hashes.items():
    assert hashlib.sha256((ROOT/'Content'/package_file(path)).read_bytes()).hexdigest()==digest, path
import sys
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ValidateGuardianMotion import validate_guardian_motion
assert validate_guardian_motion(
    {r['EnemyID']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(enemy_table))},
    {r['SkillID']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(skill_table))})==1
report = dict(status='PASS',config=CONFIG,backup=str(BACKUP),clips=[a.get_path_name() for a in [*clips.values(),slam]],
              animation_blueprint=blueprint.get_path_name(),source_hashes=source_hashes)
(OUT/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('PGGuardian Motion PASS '+str(OUT))

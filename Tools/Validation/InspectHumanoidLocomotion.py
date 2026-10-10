"""Read-only inventory of locomotion assets and their current consumers."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/HumanoidLocomotion'
out.mkdir(parents=True, exist_ok=True)
bp = unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/Animation/ABP_LocalPlayer')
task = unreal.AssetExportTask()
task.object = bp
task.filename = str(out/'player-graph.txt')
task.automated = True
task.prompt = False
task.exporter = unreal.ObjectExporterT3D()
unreal.Exporter.run_asset_export_task(task)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
result = {'blends': [], 'blueprints': [], 'sources': []}
result['player_dependencies'] = {}
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
pending = ['/Game/Blueprints/Actor/LocalPlayer/Animation/ABP_LocalPlayer']
while pending:
    path = pending.pop()
    if path in result['player_dependencies']: continue
    deps = [str(p) for p in registry.get_dependencies(path, options)]
    result['player_dependencies'][path] = deps
    pending.extend(p for p in deps if '/Game/' in p and ('ABP_' in p or 'AL_' in p or 'ALI_' in p))
for entry in registry.get_assets_by_path('/Game', recursive=True):
    path = str(entry.package_name)
    if path.endswith('BS_UnarmedLocomotion__'): continue # Unused legacy blend has a missing sample.
    cls = str(entry.asset_class_path.asset_name)
    if cls in ('BlendSpace', 'BlendSpace1D') and not path.startswith('/Game/ExternalAssets/Animations'):
        asset = unreal.load_asset(path)
        if not asset.get_editor_property('skeleton'): continue
        result['blends'].append(dict(path=path, skeleton=asset.get_editor_property('skeleton').get_path_name(),
            axes=[p.export_text() for p in asset.get_editor_property('blend_parameters')],
            samples=[s.export_text() for s in asset.get_editor_property('sample_data')],
            referencers=list(registry.get_referencers(entry.package_name, unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)))))
    if cls == 'AnimSequence' and any(k in path.lower() for k in ('walk', 'run', 'jog', 'idle')) and '/AnimationTests/' in path:
        result['sources'].append(path)
    if cls == 'Blueprint' and (path.endswith('BP_LocalPlayer') or '/MonsterVariations/BP_' in path or path.endswith('HumanoidBoss/BP_15401')):
        asset = unreal.load_asset(path)
        cdo = unreal.get_default_object(asset.generated_class())
        row = dict(path=path, mesh=cdo.mesh.get_editor_property('skeletal_mesh_asset').get_path_name(), anim=str(cdo.mesh.get_editor_property('anim_class')))
        try: row['locomotion'] = str(cdo.get_editor_property('creature_locomotion'))
        except Exception: pass
        result['blueprints'].append(row)
out = root/'Saved/HumanoidLocomotion'
out.mkdir(parents=True, exist_ok=True)
(out/'inventory.json').write_text(json.dumps(result, default=str, indent=2), encoding='utf-8')
speeds=[]
for pack,prefix in [('Sword2','AS_PGFrank_Sword2_8Way_'),('Warrior','AS_PGVelocity_8Way_')]:
    folder='/Game/Art/AnimationTests/FrankSlash/Animations/'+pack
    for entry in registry.get_assets_by_path(folder):
        name=str(entry.asset_name)
        if not name.startswith((prefix+'Run_F_',prefix+'Walk_F_')):continue
        if pack=='Sword2' and 'RootMotion' not in name:continue
        clip=unreal.load_asset(str(entry.package_name));frames=[]
        for fraction in (0,1):
            pose=clip.get_anim_pose_at_time(clip.get_play_length()*fraction,unreal.AnimPoseEvaluationOptions())
            frames.append({b:str(unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD).translation) for b in ('root','pelvis')})
        speeds.append(dict(path=str(entry.package_name),seconds=clip.get_play_length(),frames=frames))
(out/'source-speeds.json').write_text(json.dumps(speeds,indent=2),encoding='utf-8')
unreal.log('PGHumanoidLocomotion INVENTORY PASS')

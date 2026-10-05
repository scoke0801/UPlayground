"""UE 5.8 polish source adapter. Never silently adopt or reset unknown editor work.

Schema 1 owns FK rigs, retarget poses/known ops and appearance tuning. Solvers/goals
are deliberately rejected until a reproducible IK authoring adapter is implemented.
Portrait and other presentation fields outside this list remain editor-owned.
"""
import hashlib
import json
import re
import math
from pathlib import Path
import unreal
from PlayableCharacterCatalog import PLAYER_IDS, ENEMY_IDS

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'Tools/Validation/Data/PlayableCharacterPolish.json'
DEST = '/Game/DataCenter/Characters'
IDS = PLAYER_IDS + ENEMY_IDS
STAMP = 'PG.Polish.Semantic.v1'
OPS = {'IKRetargetPelvisMotionController': 'IKRetargetPelvisMotionOp',
       'IKRetargetFKChainsController': 'IKRetargetFKChainsOp'}
SCALARS = ['id', 'display_name', 'reconstruct_scaled_translations', 'head_bone']
STRUCTS = ['mesh_transform', 'head_forward_axis', 'head_right_axis']


def canonical(value):
    # FName spelling depends on the process-global name pool. Compare names/paths
    # case-insensitively and serialized floats at 1e-5 precision (not package hashes).
    def normalize(item, key=''):
        if isinstance(item, dict):
            return {k.casefold(): normalize(v, k) for k, v in item.items()}
        if isinstance(item, list): return [normalize(v, key) for v in item]
        if isinstance(item, str) and key != 'display_name':
            return re.sub(r'-?\d+\.\d+(?:[eE][+-]?\d+)?',
                          lambda m: format(round(float(m.group()), 5)+0.0, '.5f'), item.casefold())
        return item
    return json.dumps(normalize(value), sort_keys=True, ensure_ascii=False, separators=(',', ':'), allow_nan=False)


def equivalent(left, right):
    return canonical(left) == canonical(right)


def digest(value):
    return hashlib.sha256(canonical(value).encode('utf-8')).hexdigest()


def path(obj):
    return obj.get_path_name() if obj else None


def packages():
    return [DEST+'/'+prefix+identity+suffix for identity in IDS
            for prefix, suffix in [('DA_', ''), ('IK_', '_Source'), ('IK_', '_Target'), ('RTG_', '')]]


def bones(mesh):
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    return [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]


def snapshot(asset):
    if isinstance(asset, unreal.PGCharacterAppearance):
        fields = {key: str(asset.get_editor_property(key)) for key in SCALARS}
        fields['reconstruct_scaled_translations'] = bool(asset.get_editor_property('reconstruct_scaled_translations'))
        fields.update({key: asset.get_editor_property(key).export_text() for key in STRUCTS})
        fields['equipment_bones'] = {str(k): str(v) for k, v in asset.get_editor_property('equipment_bones').items()}
        fields['parts'] = [part.export_text() for part in asset.get_editor_property('parts')]
        fields['grip_profiles'] = [grip.export_text() for grip in asset.get_editor_property('grip_profiles')]
        fields['mesh'] = path(asset.get_editor_property('mesh'))
        fields['source_mesh'] = path(asset.get_editor_property('source_mesh'))
        fields['retargeter'] = asset.get_editor_property('retargeter').export_text()
        return dict(kind='appearance', fields=fields)
    if isinstance(asset, unreal.IKRigDefinition):
        ctl = unreal.IKRigController.get_controller(asset)
        if ctl.get_num_solvers() or ctl.get_all_goals():
            raise ValueError('Unmanaged IK solvers/goals: '+path(asset))
        excluded = [bone for bone in bones(ctl.get_skeletal_mesh()) if ctl.get_bone_excluded(bone)]
        if excluded:
            raise ValueError('Unmanaged excluded bones: '+path(asset))
        return dict(kind='rig', mesh=path(ctl.get_skeletal_mesh()), root=str(ctl.get_root_motion_bone()),
                    pelvis=str(ctl.get_retarget_root()), chains=[dict(name=str(c.chain_name),
                        start=str(ctl.get_retarget_chain_start_bone(c.chain_name)),
                        end=str(ctl.get_retarget_chain_end_bone(c.chain_name)),
                        goal=str(ctl.get_retarget_chain_goal(c.chain_name))) for c in ctl.get_retarget_chains()])
    if isinstance(asset, unreal.IKRetargeter):
        ctl = unreal.IKRetargeterController.get_controller(asset)
        ops = []
        for index in range(ctl.get_num_retarget_ops()):
            controller = ctl.get_op_controller(index)
            name = controller.get_class().get_name()
            if name not in OPS:
                raise ValueError('Unmanaged retarget Op '+name+': '+path(asset))
            op_name = str(ctl.get_op_name(index))
            ops.append(dict(type=OPS[name], name=op_name, parent=str(ctl.get_parent_op_by_name(op_name)),
                            settings=controller.get_settings().export_text()))
        sides = {}
        for label, side in [('source', unreal.RetargetSourceOrTarget.SOURCE), ('target', unreal.RetargetSourceOrTarget.TARGET)]:
            poses = {}
            for name, pose in ctl.get_retarget_poses(side).items():
                poses[str(name)] = dict(root=pose.get_editor_property('root_translation_offset').export_text(),
                    rotations={str(b): q.export_text() for b, q in pose.get_editor_property('bone_rotation_offsets').items()})
            sides[label] = dict(rig=path(ctl.get_ik_rig(side)), mesh=path(ctl.get_preview_mesh(side)),
                                current_pose=str(ctl.get_current_retarget_pose_name(side)), poses=poses)
        return dict(kind='retarget', ops=ops, **sides)
    raise TypeError(path(asset))


def read_source():
    data = json.loads(SOURCE.read_text(encoding='utf-8'))
    if data['schema_version'] != 1 or set(data['assets']) != set(packages()):
        raise ValueError('Unsupported schema or incomplete polish asset list')
    project = json.loads((ROOT/'UPlayground.uproject').read_text(encoding='utf-8-sig'))
    if data['engine_association'] != project['EngineAssociation']:
        raise ValueError('Polish source engine version does not match this project')
    return data


def preflight(data):
    """Inspect every managed package before the first asset mutation."""
    conflicts = []
    for package, wanted in data['assets'].items():
        asset = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None
        if not asset:
            continue
        try:
            actual = snapshot(asset)
            stamp = unreal.EditorAssetLibrary.get_metadata_tag(asset, STAMP)
            if not equivalent(actual, wanted) and stamp != digest(actual):
                conflicts.append(package+': unexported editor changes (run polish-export first)')
        except Exception as error:
            conflicts.append(str(error))
    if conflicts:
        raise ValueError('\n'.join(conflicts))


def validate_appearance(asset):
    fields = snapshot(asset)['fields']
    target = asset.get_editor_property('mesh')
    source = asset.get_editor_property('source_mesh')
    assert target and source
    available = {bone.casefold() for bone in bones(target)}
    source_bones = {bone.casefold() for bone in bones(source)}
    for src, dst in fields['equipment_bones'].items():
        assert src.casefold() in source_bones and dst.casefold() in available, (path(asset), src, dst)
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(target)
    source_component = unreal.SkeletalMeshComponent()
    source_component.set_skeletal_mesh_asset(source)
    keys = set()
    for grip in asset.get_editor_property('grip_profiles'):
        tag = str(unreal.GameplayTagLibrary.get_tag_name(grip.weapon_tag))
        socket = str(grip.source_socket)
        key = (tag.casefold(), socket.casefold())
        assert key not in keys and tag != 'None' and socket != 'None', (path(asset), key)
        keys.add(key)
        assert source_component.does_socket_exist(grip.source_socket), (path(asset), socket)
        assert component.does_socket_exist(grip.target_socket), (path(asset), str(grip.target_socket))
        transform = grip.grip_offset
        values = [getattr(vector, axis) for vector in (transform.translation, transform.scale3d) for axis in ('x','y','z')]
        values += [getattr(transform.rotation, axis) for axis in ('x','y','z','w')]
        assert all(math.isfinite(value) for value in values), path(asset)
        assert all(abs(getattr(transform.scale3d, axis)) > 1e-6 for axis in ('x','y','z')), path(asset)
        assert abs(sum(getattr(transform.rotation, axis)**2 for axis in ('x','y','z','w'))-1) < 1e-4, path(asset)
        finger_bones = set()
        for finger in grip.fingers:
            bone = str(finger.bone)
            assert bone.casefold() in available and bone.casefold() not in finger_bones, (path(asset), bone)
            finger_bones.add(bone.casefold())
            parent = str(component.get_parent_bone(finger.bone))
            while parent.casefold() not in ('none', str(grip.target_socket).casefold()):
                parent = str(component.get_parent_bone(parent))
            assert parent.casefold() == str(grip.target_socket).casefold(), (path(asset), 'finger outside hand', bone)
            assert all(math.isfinite(getattr(finger.reference_rotation_offset, axis)) for axis in ('pitch','yaw','roll')), (path(asset), bone)


def struct_value(cls, text):
    value = cls()
    value.import_text(text)
    # Reject malformed/partially imported input rather than accepting default settings.
    if not equivalent(value.export_text(), text):
        raise ValueError('Noncanonical or unsupported '+cls.__name__+' source: '+text[:100])
    return value


def apply_rig(asset, definition):
    ctl = unreal.IKRigController.get_controller(asset)
    mesh = unreal.load_asset(definition['mesh'])
    assert mesh and ctl.set_skeletal_mesh(mesh), definition['mesh']
    available = {bone.casefold() for bone in bones(mesh)}
    assert definition['pelvis'].casefold() in available
    for c in definition['chains']:
        assert c['start'].casefold() in available and c['end'].casefold() in available and c['goal'] == 'None', c
    for c in list(ctl.get_retarget_chains()):
        assert ctl.remove_retarget_chain(c.chain_name)
    assert ctl.set_retarget_root(definition['pelvis'])
    assert ctl.set_root_motion_bone(definition['root'])
    for c in definition['chains']:
        assert str(ctl.add_retarget_chain(c['name'], c['start'], c['end'], c['goal'])).casefold() == c['name'].casefold()


def apply_retarget(asset, definition):
    ctl = unreal.IKRetargeterController.get_controller(asset)
    assert all(row['type'] in OPS.values() for row in definition['ops']), 'Unsupported Op in source'
    ctl.remove_all_ops()
    for label, side in [('source', unreal.RetargetSourceOrTarget.SOURCE), ('target', unreal.RetargetSourceOrTarget.TARGET)]:
        row = definition[label]
        rig, mesh = unreal.load_asset(row['rig']), unreal.load_asset(row['mesh'])
        assert rig and mesh
        ctl.set_ik_rig(side, rig)
        ctl.set_preview_mesh(side, mesh)
        for name in list(ctl.get_retarget_poses(side)):
            if str(name) not in row['poses']:
                assert ctl.remove_retarget_pose(name, side)
        for name, pose in row['poses'].items():
            if name not in [str(n) for n in ctl.get_retarget_poses(side)]:
                assert str(ctl.create_retarget_pose(name, side)) == name
            assert ctl.set_current_retarget_pose(name, side)
            ctl.reset_retarget_pose(name, [], side)
            ctl.set_root_offset_in_retarget_pose(struct_value(unreal.Vector, pose['root']), side)
            available = {bone.casefold() for bone in bones(mesh)}
            for bone, rotation in pose['rotations'].items():
                assert bone.casefold() in available, bone
                ctl.set_rotation_offset_for_retarget_pose_bone(bone, struct_value(unreal.Quat, rotation), side)
        assert ctl.set_current_retarget_pose(row['current_pose'], side)
    for row in definition['ops']:
        index = ctl.add_retarget_op('/Script/IKRig.'+row['type'])
        assert index >= 0
        ctl.run_op_initial_setup(index)
        assert str(ctl.set_op_name(row['name'], index)).casefold() == row['name'].casefold()
        controller = ctl.get_op_controller(index)
        controller.set_settings(struct_value(type(controller.get_settings()), row['settings']))
    for row in definition['ops']:
        if row['parent'] != 'None':
            assert ctl.set_parent_op_by_name(row['name'], row['parent'])


def apply_appearance(asset, definition):
    fields = definition['fields']
    for key in SCALARS:
        asset.set_editor_property(key, fields[key])
    for key in STRUCTS:
        asset.set_editor_property(key, struct_value(type(asset.get_editor_property(key)), fields[key]))
    for key in ['mesh', 'source_mesh']:
        mesh = unreal.load_asset(fields[key])
        assert mesh, fields[key]
        asset.set_editor_property(key, mesh)
    asset.set_editor_property('retargeter', struct_value(unreal.SoftObjectPath, fields['retargeter']))
    asset.set_editor_property('equipment_bones', fields['equipment_bones'])
    asset.set_editor_property('parts', [struct_value(unreal.PGAppearancePart, p) for p in fields['parts']])
    asset.set_editor_property('grip_profiles', [struct_value(unreal.PGAppearanceGripProfile, p) for p in fields['grip_profiles']])


def export_source():
    """Export to a review file; runner atomically adopts it after editor exit."""
    result = dict(schema_version=1, engine_association='5.8', assets={})
    for package in packages():
        asset = unreal.load_asset(package)
        assert asset, package
        result['assets'][package] = snapshot(asset)
    return result


def validate_source(data):
    errors = []
    for package, expected in data['assets'].items():
        asset = unreal.load_asset(package)
        if not asset or not equivalent(snapshot(asset), expected):
            errors.append(package)
        elif expected['kind'] == 'appearance':
            validate_appearance(asset)
    if errors:
        raise ValueError('Semantic reload mismatch: '+', '.join(errors))

"""Fresh-process semantic reload and editor-owned field preservation check."""
import json
import os
import sys
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(root/'Tools/Validation'))
from PlayableCharacterPolish import read_source, validate_source, preflight, snapshot, path, equivalent, apply_appearance, validate_appearance
from PlayableCharacterTransaction import write_json

output = Path(os.environ['PG_CHARACTER_RUN'])
source = read_source()
validate_source(source)
# Exercise a non-empty grip source with the real reflected struct, not just [] in
# the production defaults. Keep the authored asset unchanged on disk and in memory.
appearance=unreal.load_asset('/Game/DataCenter/Characters/DA_Bokusei')
before=snapshot(appearance)
try:
    tag=unreal.GameplayTag()
    tag.import_text('(TagName="Weapon.Sword")')
    grip=unreal.PGAppearanceGripProfile()
    grip.set_editor_property('weapon_tag',tag)
    grip.set_editor_property('source_socket','hand_r')
    grip.set_editor_property('target_socket',appearance.get_editor_property('equipment_bones')['hand_r'])
    grip.set_editor_property('grip_offset',unreal.Transform(location=unreal.Vector(1,2,3)))
    finger=unreal.PGAppearanceGripFinger()
    finger.bone=appearance.get_editor_property('equipment_bones')['index_01_r']
    finger.reference_rotation_offset=unreal.Rotator(pitch=0,yaw=60,roll=0)
    grip.fingers=[finger]
    appearance.set_editor_property('grip_profiles',[grip])
    exported=snapshot(appearance)
    appearance.set_editor_property('grip_profiles',[])
    apply_appearance(appearance,exported)
    assert equivalent(snapshot(appearance),exported), 'Non-empty grip roundtrip failed'
    validate_appearance(appearance)
    for bad_bone, duplicate in [('PGMissingFinger',False),(str(grip.target_socket),False),(str(finger.bone),True)]:
        invalid=unreal.PGAppearanceGripFinger()
        invalid.bone=bad_bone
        grip.fingers=[invalid,invalid] if duplicate else [invalid]
        appearance.set_editor_property('grip_profiles',[grip])
        rejected=False
        try: validate_appearance(appearance)
        except AssertionError: rejected=True
        assert rejected, ('Invalid finger pose accepted',bad_bone,duplicate)
finally:
    apply_appearance(appearance,before)
configure = output/'configure.json'
if configure.exists():
    report = json.loads(configure.read_text(encoding='utf-8'))
    for package, portrait in report['preserved_portraits'].items():
        assert path(unreal.load_asset(package).get_editor_property('portrait')) == portrait, package
    for package, before in report.get('preserved_gameplay_rows', {}).items():
        table=unreal.load_asset(package)
        after=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        by_name={row['Name']:row for row in after}
        assert all(row['Name'] in by_name and equivalent(row,by_name[row['Name']]) for row in before), package
# Probe edits in memory only. The same preflight used by configure must reject them
# before any write, including a known UE Op unsupported by this source schema.
asset = unreal.load_asset('/Game/DataCenter/Characters/RTG_Bokusei')
ctl = unreal.IKRetargeterController.get_controller(asset)
op = ctl.get_op_controller(0)
original = op.get_settings()
changed = op.get_settings()
changed.set_editor_property('translation_alpha', .731)
try:
    op.set_settings(changed)
    rejected = False
    try: preflight(source)
    except ValueError: rejected = True
    assert rejected, 'Unexported editor tuning was not detected'
finally:
    op.set_settings(original)
index = ctl.add_retarget_op('/Script/IKRig.IKRetargetRunIKRigOp')
assert index >= 0
try:
    rejected = False
    try: snapshot(asset)
    except ValueError: rejected = True
    assert rejected, 'Unknown Op was silently adopted'
finally:
    assert ctl.remove_retarget_op(index)
validate_source(source)
write_json(output/'polish-validate.json', dict(status='PASS', assets=len(source['assets']), semantic_reload=True,
                                            unexported_edit_rejected=True, unknown_op_rejected=True,
                                            nonempty_grip_roundtrip=True, finger_pose_roundtrip=True,
                                            invalid_finger_bone_rejected=True, nonfinger_bone_rejected=True,
                                            duplicate_finger_rejected=True))

"""Read-only skeleton/API audit for the isolated toon animation test."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir()).resolve() / 'Saved/ToonTest'
out.mkdir(parents=True, exist_ok=True)
result = {}
for label, path in {
    'source': '/Game/ExternalAssets/Characters/ElfSelena/BaseMesh/SK_ElfSelena',
    'target': '/Game/Art/ToonTest/Inori/SK_Inori_ToonTest',
}.items():
    mesh = unreal.load_asset(path)
    assert isinstance(mesh, unreal.SkeletalMesh), path
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    rig = unreal.IKRigDefinition()
    controller = unreal.IKRigController.get_controller(rig)
    assert controller.set_skeletal_mesh(mesh)
    bones = []
    for i in range(component.get_num_bones()):
        name = component.get_bone_name(i)
        transform = controller.get_ref_pose_transform_of_bone(name)
        bones.append({'name': str(name), 'parent': str(component.get_parent_bone(name)),
                      'position': [transform.translation.x, transform.translation.y, transform.translation.z],
                      'rotation': str(transform.rotation)})
    result[label] = {'mesh': path, 'skeleton': mesh.get_editor_property('skeleton').get_path_name(), 'bones': bones}

classes = ['IKRigDefinitionFactory', 'IKRetargetFactory', 'IKRigController',
           'IKRetargeterController', 'IKRetargetBatchOperation', 'IKRetargetBatchOperationInputs',
           'RetargetSourceOrTarget', 'RetargetAutoAlignMethod', 'AutoMapChainType',
           'IKRetargetFKChainsController', 'IKRetargetPelvisMotionController',
           'LevelEditorSubsystem', 'Rotator']
result['rotation_positional'] = str(unreal.Rotator(-55, -45, 0))
result['rotation_named'] = str(unreal.Rotator(pitch=-55, yaw=-45, roll=0))
docs = []
for name in classes:
    cls = getattr(unreal, name, None)
    docs.append(name + '\n' + str(getattr(cls, '__doc__', None)))
    if cls:
        for member in dir(cls):
            if not member.startswith('_'):
                docs.append(member + ': ' + str(getattr(getattr(cls, member), '__doc__', None)))
(out / 'retarget_api.txt').write_text('\n\n'.join(docs), encoding='utf-8')
(out / 'retarget_audit.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log('PG Toon retarget skeleton audit complete')

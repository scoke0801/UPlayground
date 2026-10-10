"""Additional identities in the comparison map; never modify source appearances."""
import math
import unreal

DEST = '/Game/Art/ToonTest/BokuseiShadingComparison'
GUESTS = [('Arin', '아린', 2600), ('Hwarin', '화련', 5200), ('LianLian', 'Lianlian', 7800)]
TAG = 'PGShadingGuest'


def bokusei_idle(identity, appearance):
    """Retarget the exact displayed Bokusei clip, using comparison-owned rigs."""
    eal = unreal.EditorAssetLibrary
    folder = DEST+'/Animation'
    name = 'PGGuestPose_'+identity+'_PGBokusei_AS_Anime_KC_Idle'
    if eal.does_asset_exist(folder+'/'+name):
        return unreal.load_asset(folder+'/'+name)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    source = unreal.load_asset('/Game/DataCenter/Characters/DA_Bokusei').get_editor_property('mesh')
    target = appearance.get_editor_property('mesh')
    chains = [('Spine','Spine','UpperChest'), ('Neck','Neck','Neck'), ('Head','Head','Head')]
    for side in ['L', 'R']:
        chains += [('Shoulder_'+side,'Shoulder_'+side,'Shoulder_'+side),
                   ('Arm_'+side,'UpperArm_'+side,'Hand_'+side),
                   ('Leg_'+side,'UpperLeg_'+side,'Foot_'+side), ('Toe_'+side,'Toes_'+side,'Toes_'+side)]
        for finger in ['Thumb','Index','Middle','Ring','Little']:
            chains.append((finger+'_'+side,finger+'Proximal_'+side,finger+'Distal_'+side))

    def own(asset_name, cls, factory):
        path = folder+'/'+asset_name
        return unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(asset_name,folder,cls,factory)

    def rig(asset_name, mesh):
        asset = own(asset_name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
        controller = unreal.IKRigController.get_controller(asset)
        assert controller.set_skeletal_mesh(mesh)
        for chain in list(controller.get_retarget_chains()): controller.remove_retarget_chain(chain.chain_name)
        assert controller.set_retarget_root('Hips')
        reference = unreal.AnimPoseExtensions.get_reference_pose(mesh.get_editor_property('skeleton'))
        bones = {str(b).replace('_','').lower(): str(b) for b in unreal.AnimPoseExtensions.get_bone_names(reference)}
        def resolve(name):
            key = name.replace('_','').lower()
            if key == 'upperchest' and key not in bones: key = 'chest'
            if key not in bones: key = key.replace('proximal','1').replace('distal','3')
            assert key in bones, (asset_name, name)
            return bones[key]
        for chain, start, end in chains:
            assert str(controller.add_retarget_chain(chain,resolve(start),resolve(end),'None')) == chain, (asset_name,chain)
        assert eal.save_loaded_asset(asset)
        return asset

    source_rig = rig('IK_PGGuestPose_Bokusei',source)
    target_rig = rig('IK_PGGuestPose_'+identity,target)
    retarget = own('RTG_PGGuestPose_'+identity,unreal.IKRetargeter,unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(retarget)
    s, t = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    controller.remove_all_ops()
    controller.set_ik_rig(s,source_rig); controller.set_ik_rig(t,target_rig)
    controller.set_preview_mesh(s,source); controller.set_preview_mesh(t,target)
    for op in ['IKRetargetPelvisMotionOp','IKRetargetFKChainsOp']:
        index = controller.add_retarget_op('/Script/IKRig.'+op)
        assert index >= 0
        controller.run_op_initial_setup(index)
    controller.assign_ik_rig_to_all_ops(s,source_rig)
    controller.assign_ik_rig_to_all_ops(t,target_rig)
    for chain, _, _ in chains: assert controller.set_source_chain(chain,chain)
    controller.reset_retarget_pose('Default Pose',[],t)
    controller.auto_align_all_bones(t,unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    assert eal.save_loaded_asset(retarget)
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [eal.find_asset_data('/Game/Art/ToonTest/Bokusei/Animation/PGBokusei_AS_Anime_KC_Idle')]
    inputs.source_mesh, inputs.target_mesh, inputs.ik_retarget_asset = source, target, retarget
    inputs.prefix, inputs.target_path = 'PGGuestPose_'+identity+'_', folder
    inputs.include_referenced_assets = False
    assert len(unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)) == 1
    idle = unreal.load_asset(folder+'/'+name)
    assert idle and eal.save_loaded_asset(idle)
    return idle


def restore_import_scale(idle, appearance):
    """Match the runtime appearance path for the two imported meter-unit rigs."""
    if not appearance.get_editor_property('reconstruct_scaled_translations'):
        return
    reference = unreal.AnimPoseExtensions.get_reference_pose(appearance.get_editor_property('mesh').get_editor_property('skeleton'))
    names = list(unreal.AnimPoseExtensions.get_bone_names(reference))
    assert str(names[0]) == 'root' and str(names[1]) == 'Hips'
    space = unreal.AnimPoseSpaces.LOCAL
    scale = unreal.AnimPoseExtensions.get_bone_pose(reference, names[0], space).scale3d
    options = unreal.AnimPoseEvaluationOptions()
    first = idle.get_anim_pose_at_time(0., options)
    current = unreal.AnimPoseExtensions.get_bone_pose(first, names[0], space).scale3d
    if (current-scale).length() < .001:
        return
    assert (current-unreal.Vector(1, 1, 1)).length() < .001
    frames = unreal.AnimationLibrary.get_num_frames(idle)
    poses = [idle.get_anim_pose_at_time(idle.get_play_length()*i/frames, options) for i in range(frames+1)]
    controller = idle.get_editor_property('controller')
    controller.open_bracket('Restore imported guest rig scale', should_transact=False)
    try:
        for bone in names[:2]:
            transforms = [unreal.AnimPoseExtensions.get_bone_pose(pose, bone, space) for pose in poses]
            positions = [t.translation if bone == names[0] else unreal.Vector(t.translation.x/scale.x, t.translation.y/scale.y, t.translation.z/scale.z) for t in transforms]
            scales = [scale if bone == names[0] else t.scale3d for t in transforms]
            assert controller.set_bone_track_keys(bone, positions, [t.rotation for t in transforms], scales, should_transact=False)
    finally:
        controller.close_bracket(should_transact=False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(idle)


def add_guests(actors, key, prop, label_material):
    # Preflight all identities before touching the level. Each rig needs its own
    # animation; sharing Bokusei's leader pose would corrupt different skeletons.
    loaded = []
    for identity, label, x in GUESTS:
        appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_'+identity)
        assert appearance, identity
        mesh = appearance.get_editor_property('mesh')
        assert mesh and not appearance.get_editor_property('parts'), identity
        idle = bokusei_idle(identity, appearance)
        restore_import_scale(idle, appearance)
        loaded.append((identity, label, x, appearance, mesh, idle))
    key.light_component.set_editor_property('forward_shading_priority', 1)
    from BokuseiGuestStages import build_stages, STAGE_IDS
    variants = {identity: build_stages(identity, mesh) for identity, _, _, _, mesh, _ in loaded}
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            actors.destroy_actor(actor)
    rows = []
    stage = unreal.load_asset(DEST+'/Materials/M_PGComparison_Stage')
    # These signs belong to the original stage fixture. Reuse them rather than
    # reimporting textures whose bulk data is already held by the loaded map.
    labels = {name: unreal.load_asset(DEST+'/Labels/M_PGComparison_'+name) for name in STAGE_IDS}
    assert all(labels.values())
    backdrop = unreal.load_asset(DEST+'/Materials/M_PGComparison_Backdrop')
    for identity, label, x, appearance, mesh, idle in loaded:
        materials, hair = variants[identity]
        decorations = [
            prop('바닥 · '+label, 'Cube', (x, 0, -12), (26, 20, .12), stage),
            prop('배경 · '+label, 'Cube', (x, -160, 220), (26, .08, 7), backdrop),
            prop('모델 · '+label, 'Plane', (x, 0, 270), (3.6, .72, 1), label_material(identity), unreal.Rotator(roll=90))]
        leader = None
        for index, stage_materials in enumerate(materials):
            position = unreal.Vector(x+(index-3.5)*220, 0, 0)
            actor = actors.spawn_actor_from_class(unreal.PGToonPreviewActor, position)
            actor.set_actor_label(label+' · '+str(index+1)+'단계')
            actor.set_folder_path('추가 모델/'+label)
            tags = [TAG, 'PGShadingGuest_'+identity, 'PGShadingGuestStage_'+identity+'_'+str(index)]
            if index in [5, 7]: tags.append('PGShadingGuestWorld')
            actor.set_editor_property('tags', tags)
            component = actor.skeletal_mesh_component
            component.set_skeletal_mesh_asset(mesh)
            component.set_relative_transform(appearance.get_editor_property('mesh_transform'), False, True)
            actor.set_actor_location(position, False, True)
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_forced_lod(1)
            component.set_render_custom_depth(index >= 4)
            component.set_custom_depth_stencil_value(73 if index >= 4 else 0)
            for slot, material in enumerate(stage_materials): component.set_material(slot, material)
            component.override_animation_data(idle, True, True, 1.25, 1.)
            component.set_update_animation_in_editor(True)
            component.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
            if leader: component.set_leader_pose_component(leader)
            else: leader = component
            presentation = actor.toon_presentation
            presentation.set_editor_property('key_light', key)
            for name in ['head_bone', 'head_forward_axis', 'head_right_axis']:
                presentation.set_editor_property(name, appearance.get_editor_property(name))
            if index in [5, 7]:
                proxy = actor.hair_shadow_proxy
                proxy.set_skeletal_mesh_asset(mesh)
                proxy.set_forced_lod(1)
                proxy.set_leader_pose_component(leader)
                for slot, material in enumerate(hair): proxy.set_material(slot, material)
                proxy.set_cast_shadow(True)
            decorations.append(prop('받침대 · '+label, 'Cylinder', (position.x, 0, -3), (1.45, 1.45, .06), stage))
            sign = prop('단계 · '+label, 'Plane', (position.x, 0, 205), (2., .4, 1), labels[STAGE_IDS[index]], unreal.Rotator(roll=90))
            decorations.append(sign)
            if index >= 4:
                caster = prop('가림막 · '+label, 'Cube', (position.x-39, 50, 230), (1.3, .5, .14), stage)
                caster.set_editor_property('tags', [TAG, 'PGShadingGuestShadowCaster'])
                caster.set_folder_path('추가 모델/'+label)
                c = caster.static_mesh_component
                c.set_mobility(unreal.ComponentMobility.MOVABLE)
                c.set_editor_property('cast_hidden_shadow', True)
                c.set_cast_shadow(False)
                c.set_visibility(False)
        for decoration in decorations:
            decoration.set_editor_property('tags', [TAG])
            decoration.set_folder_path('추가 모델/'+label)
            if 'Plane' in decoration.static_mesh_component.get_editor_property('static_mesh').get_name():
                decoration.static_mesh_component.set_cast_shadow(False)
        rows.append(dict(id=identity, label=label, mesh=mesh.get_path_name(), animation=idle.get_path_name(), stages=8))
    return rows


def validate_guests(actors):
    rows = []
    lib = unreal.MaterialEditingLibrary
    for identity, label, x in GUESTS:
        matches = [a for a in actors if a.actor_has_tag('PGShadingGuest_'+identity)]
        assert len(matches) == 8, (identity, len(matches))
        appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_'+identity)
        leader = next(a.skeletal_mesh_component for a in matches if a.actor_has_tag('PGShadingGuestStage_'+identity+'_0'))
        idle = leader.get_editor_property('animation_data').anim_to_play
        assert idle and idle.get_name().startswith('PGGuestPose_'+identity+'_'), identity
        source_idle = unreal.load_asset('/Game/Art/ToonTest/Bokusei/Animation/PGBokusei_AS_Anime_KC_Idle')
        assert abs(idle.get_play_length()-source_idle.get_play_length()) < .001
        angles = []
        for time in [0., 1.25, source_idle.get_play_length()*.75]:
            poses = [clip.get_anim_pose_at_time(time,unreal.AnimPoseEvaluationOptions()) for clip in [source_idle,idle]]
            names = [{str(b).replace('_','').lower(): b for b in unreal.AnimPoseExtensions.get_bone_names(pose)} for pose in poses]
            for side in ['l','r']:
                for start,end in [('upperarm','lowerarm'),('lowerarm','hand'),('upperleg','lowerleg'),('lowerleg','foot')]:
                    directions = []
                    for pose,bones in zip(poses,names):
                        points = [unreal.AnimPoseExtensions.get_bone_pose(pose,bones[bone+side],unreal.AnimPoseSpaces.WORLD).translation for bone in [start,end]]
                        delta = points[1]-points[0]
                        assert delta.length() > .001
                        directions.append(delta/delta.length())
                    a,b = directions
                    dot = a.x*b.x+a.y*b.y+a.z*b.z
                    angles.append(math.degrees(math.acos(max(-1.,min(1.,dot)))))
        assert max(angles) < 5., (identity, 'Bokusei limb pose mismatch', max(angles))
        for index in range(8):
            actor = next(a for a in matches if a.actor_has_tag('PGShadingGuestStage_'+identity+'_'+str(index)))
            c = actor.skeletal_mesh_component
            location = actor.get_actor_location()
            assert abs(location.x-(x+(index-3.5)*220)) < .01 and abs(location.y) < .01, identity
            assert c.get_skeletal_mesh_asset() == appearance.get_editor_property('mesh'), identity
            data = c.get_editor_property('animation_data')
            assert data.anim_to_play == idle and abs(data.saved_position-1.25) < .001, identity
            assert c.get_editor_property('leader_pose_component') == (leader if index else None), identity
            assert c.get_editor_property('render_custom_depth') == (index >= 4)
            assert actor.toon_presentation.key_light
            sdf_count = 0
            for slot in range(c.get_num_materials()):
                material = c.get_material(slot)
                assert material.get_path_name().startswith(DEST+'/'), (identity, index, slot)
                parent = material
                while isinstance(parent, unreal.MaterialInstanceConstant):
                    parent = parent.get_editor_property('parent')
                expected = unreal.MaterialShadingModel.MSM_DEFAULT_LIT if index in [0, 5, 7] else unreal.MaterialShadingModel.MSM_UNLIT
                assert parent.get_editor_property('shading_model') == expected, (identity, index, slot)
                if index in [1, 2]:
                    assert lib.get_material_instance_scalar_parameter_value(material, 'RimStrength') == 0
                    assert lib.get_material_instance_scalar_parameter_value(material, 'SpecularStrength') == 0
                sdf_count += lib.get_material_instance_scalar_parameter_value(material, 'FaceSDFEnabled') > .5 if index else 0
            assert sdf_count == (1 if index >= 6 else 0), (identity, index, sdf_count)
            if index in [5, 7]:
                assert actor.hair_shadow_proxy.get_skeletal_mesh_asset() == c.get_skeletal_mesh_asset()
                assert actor.hair_shadow_proxy.get_editor_property('cast_shadow')
        rows.append(dict(id=identity, stages=8, pose_max_limb_angle_degrees=max(angles)))
    return rows

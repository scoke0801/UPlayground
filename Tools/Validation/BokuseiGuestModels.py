"""Additional identities in the comparison map; never modify source appearances."""
import unreal

DEST = '/Game/Art/ToonTest/BokuseiShadingComparison'
GUESTS = [('Arin', '아린', -440), ('Hwarin', '화련', 0), ('LianLian', 'Lianlian', 440)]
TAG = 'PGShadingGuest'


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
    frames = idle.get_data_model().get_number_of_frames()
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
        idle_path = DEST+'/Animation/PGGuest_'+identity+'_AS_Anime_KC_Idle'
        idle = unreal.load_asset(idle_path) if unreal.EditorAssetLibrary.does_asset_exist(idle_path) else None
        if not idle:
            inputs = unreal.IKRetargetBatchOperationInputs()
            inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data('/Game/Art/AnimationTests/AnimeKatana/Animations/AS_Anime_KC_Idle')]
            inputs.source_mesh = appearance.get_editor_property('source_mesh')
            inputs.target_mesh = mesh
            inputs.ik_retarget_asset = unreal.load_asset('/Game/DataCenter/Characters/RTG_'+identity)
            inputs.prefix = 'PGGuest_'+identity+'_'
            inputs.target_path = DEST+'/Animation'
            inputs.include_referenced_assets = False
            result = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
            assert len(result) == 1, identity
            idle = unreal.load_asset(idle_path)
            assert idle and unreal.EditorAssetLibrary.save_loaded_asset(idle), identity
        restore_import_scale(idle, appearance)
        loaded.append((identity, label, x, appearance, mesh, idle))
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            actors.destroy_actor(actor)
    rows = []
    stage = unreal.load_asset(DEST+'/Materials/M_PGComparison_Stage')
    for identity, label, x, appearance, mesh, idle in loaded:
        actor = actors.spawn_actor_from_class(unreal.PGToonPreviewActor, unreal.Vector(x, 650, 0))
        actor.set_actor_label('추가 모델 · '+label)
        actor.set_folder_path('추가 모델')
        actor.set_editor_property('tags', [TAG, 'PGShadingGuest_'+identity])
        component = actor.skeletal_mesh_component
        component.set_skeletal_mesh_asset(mesh)
        component.set_relative_transform(appearance.get_editor_property('mesh_transform'), False, True)
        # SkeletalMeshActor uses this component as its root: applying the mesh
        # transform above also changes the actor's world location.
        actor.set_actor_location(unreal.Vector(x, 650, 0), False, True)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_forced_lod(1)
        component.set_render_custom_depth(True)
        component.set_custom_depth_stencil_value(73)
        component.override_animation_data(idle, True, True, 1.25, 1.)
        component.set_update_animation_in_editor(True)
        component.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        presentation = actor.toon_presentation
        presentation.set_editor_property('key_light', key)
        for name in ['head_bone', 'head_forward_axis', 'head_right_axis']:
            presentation.set_editor_property(name, appearance.get_editor_property(name))
        pedestal = prop('받침대 · '+label, 'Cylinder', (x, 650, -3), (1.8, 1.8, .06), stage)
        sign = prop('안내 · '+label, 'Plane', (x, 650, 210), (2.7, .54, 1), label_material(identity), unreal.Rotator(roll=90))
        sign.static_mesh_component.set_cast_shadow(False)
        for decoration in [pedestal, sign]:
            decoration.set_editor_property('tags', [TAG])
            decoration.set_folder_path('추가 모델')
        rows.append(dict(id=identity, label=label, mesh=mesh.get_path_name(), animation=idle.get_path_name()))
    return rows


def validate_guests(actors):
    rows = []
    for identity, label, x in GUESTS:
        matches = [a for a in actors if a.actor_has_tag('PGShadingGuest_'+identity)]
        assert len(matches) == 1, identity
        actor = matches[0]
        appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_'+identity)
        c = actor.skeletal_mesh_component
        location = actor.get_actor_location()
        assert abs(location.x-x) < .01 and abs(location.y-650) < .01, identity
        assert c.get_skeletal_mesh_asset() == appearance.get_editor_property('mesh'), identity
        idle = c.get_editor_property('animation_data').anim_to_play
        assert idle, identity
        pose = idle.get_anim_pose_at_time(1.25, unreal.AnimPoseEvaluationOptions())
        head = unreal.AnimPoseExtensions.get_bone_pose(pose, appearance.get_editor_property('head_bone'), unreal.AnimPoseSpaces.WORLD).translation
        foot = unreal.AnimPoseExtensions.get_bone_pose(pose, 'Foot_L', unreal.AnimPoseSpaces.WORLD).translation
        assert 90 < head.z-foot.z < 200, (identity, head, foot)
        assert not c.get_editor_property('leader_pose_component'), identity
        assert c.get_editor_property('render_custom_depth') and c.get_editor_property('custom_depth_stencil_value') == 73
        assert actor.toon_presentation.key_light
        for index, slot in enumerate(c.get_skeletal_mesh_asset().get_editor_property('materials')):
            assert c.get_material(index) == slot.material_interface, (identity, index)
        rows.append(identity)
    return rows

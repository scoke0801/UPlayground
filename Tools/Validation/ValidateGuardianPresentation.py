"""Read-only guardian art, animation and gameplay-preservation checks."""
import json
import unreal

def validate_guardian_presentation(enemies,skills,baseline=None):
    path='/Game/DataCenter/Guardian/DA_PGGuardianPresentation'
    configured=enemies[15103].get('Presentation','None') not in ('None','',None)
    if not configured and not unreal.EditorAssetLibrary.does_asset_exist(path):return 0
    assert configured,'Guardian presentation binding was removed'
    data=unreal.load_asset(enemies[15103]['Presentation'])
    assert isinstance(data,unreal.PGEnemyPresentationData)
    pieces=data.get_editor_property('armor'); assert len(pieces)==3
    assert len({str(p.get_editor_property('name')) for p in pieces})==3
    cdo=unreal.get_default_object(unreal.load_class(None,enemies[15103]['ActorClass']))
    mesh=cdo.get_editor_property('mesh')
    for piece in pieces:
        assert mesh.does_socket_exist(piece.get_editor_property('socket'))
        part=piece.get_editor_property('mesh'); assert isinstance(part,unreal.StaticMesh)
        assert part.get_num_triangles(0)<500
        assert len(part.get_editor_property('static_materials'))==1
        material=part.get_material(0)
        assert isinstance(unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,unreal.MaterialProperty.MP_BASE_COLOR),unreal.MaterialExpressionVertexColor),'Armor vertex colors are disconnected'
        geometry=part.get_editor_property('body_setup').get_editor_property('agg_geom')
        assert all(len(geometry.get_editor_property(k))==0 for k in ['box_elems','sphere_elems','sphyl_elems','convex_elems'])
    shield=pieces[0]
    assert shield.get_editor_property('guard_transform')!=shield.get_editor_property('recovery_transform')
    for field in ['windup_sound','aim_lock_sound','recovery_sound','guard_hit_sound','guard_hit_vfx']:
        assert data.get_editor_property(field),field
    skill=skills[15103]
    assert skill['bSyncMontageToPattern'] and 0<=skill['WindupMontageFraction']<skill['ImpactMontageFraction']<1
    assert 0<skill['ImpactVFXScale']<1 and not skill['bHeavyImpactFeedback']
    montage=unreal.load_asset(skill['ElitePresentationMontage'])
    assert montage.get_editor_property('skeleton')==mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
    assert not unreal.AnimationLibrary.get_animation_notify_events(montage),'Presentation montage must never drive damage or weapon collision'
    for track in montage.get_editor_property('slot_anim_tracks'):
        for segment in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
            sequence=segment.get_editor_property('anim_reference')
            assert not unreal.AnimationLibrary.get_animation_notify_events(sequence),'Underlying sequence carries gameplay notifies'
    from ValidateGuardianMotion import validate_guardian_motion
    validate_guardian_motion(enemies,skills)
    # Only an authoring operation supplies a baseline. Routine QA must permit
    # later intentional balance work rather than freezing all tables forever.
    if baseline is not None:
        before=baseline
        for group,current,id_key,allowed in [
            ('enemies',enemies,'EnemyID',{'Presentation'}),
            ('skills',skills,'SkillID',{'ElitePresentationMontage','bSyncMontageToPattern','WindupMontageFraction','ImpactMontageFraction','ImpactVFXScale','bHeavyImpactFeedback','SlamVFX','AttackSound'})]:
            for row in before[group]:
                actual=current[row[id_key]]
                for key,value in row.items():
                    if row[id_key]==15103 and key in allowed:continue
                    assert actual.get(key)==value,('Unrelated gameplay value changed',group,row[id_key],key)
    unreal.log('PGGuardian VALIDATION PASS armor=3 baseline_checked='+str(baseline is not None))
    return 1

if __name__=='__main__':
    def rows(path,key):
        return {r[key]:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))}
    assert validate_guardian_presentation(rows('/Game/DataCenter/DataTables/Actor/DT_Enemy','EnemyID'),rows('/Game/DataCenter/DataTables/Skill/DT_Skill','SkillID'))==1

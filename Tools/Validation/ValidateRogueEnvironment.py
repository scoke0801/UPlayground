"""Read-only validation of the saved RogueArena environment replacement."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/RogueEnvironment'
replacement=json.loads((out/'replacement.json').read_text(encoding='utf-8'))
assert replacement['applied'] and len(replacement['changes'])==30
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Maps/RogueArena')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_label={a.get_actor_label():a for a in actors}
before=json.loads((out/'before.json').read_text(encoding='utf-8'))
assert len(actors)==len(before), 'Actor count changed'
assert {a.get_path_name() for a in actors}=={a['path'] for a in before}, 'Actor identities changed'
def vec(v): return [round(v.x,5),round(v.y,5),round(v.z,5)]
checks=[]
for row in replacement['changes']:
    actor=by_label[row['actor']]
    comp=actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh=comp.get_editor_property('static_mesh')
    assert mesh.get_path_name()==row['mesh'],row['actor']
    assert vec(actor.get_actor_location())==row['location_after']
    assert vec(actor.get_actor_scale3d())==row['scale_after']
    assert str(comp.get_collision_enabled())==row['collision_after'],row['actor']
    assert all(comp.get_material(i) for i in range(comp.get_num_materials()))
    if row['actor'].startswith('SM_Cube'):
        body=mesh.get_editor_property('body_setup')
        prototype=unreal.load_asset('/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_Cube').get_editor_property('body_setup')
        assert body.get_editor_property('agg_geom').export_text()==prototype.get_editor_property('agg_geom').export_text()
        assert body.get_editor_property('collision_trace_flag')==prototype.get_editor_property('collision_trace_flag')
    if row['actor']=='SM_Cube20':
        assert not comp.get_editor_property('visible')
        assert comp.get_editor_property('hidden_in_game')
    checks.append(row['actor'])
rendered=[]
for actor in actors:
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh=comp.get_editor_property('static_mesh')
        if not mesh or not comp.get_editor_property('visible') or comp.get_editor_property('hidden_in_game'):continue
        path=mesh.get_path_name()
        assert '/LevelPrototyping/' not in path and not path.startswith('/Engine/BasicShapes/'),(actor.get_actor_label(),path)
        if path.startswith('/Game/Environment/RogueArena/'): rendered.append(actor.get_actor_label())
assert len(rendered)==29
report={'status':'PASS','checked_replacements':len(checks),'visible_environment_actors':len(rendered),'actor_count_preserved':len(actors),'prototype_collision_preserved':True,'visible_placeholders_remaining':0}
(out/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('PG_ENVIRONMENT_VALIDATION_PASS '+json.dumps(report))

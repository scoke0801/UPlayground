"""Apply reviewed face candidates and author a separate improved comparison map."""
import json
import os
from pathlib import Path
import sys
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterTransaction import Transaction,write_json
RUN=Path(os.environ['PG_TOON_UPGRADE_RUN'])
SOURCE=ROOT/'Tools/Art/ToonTest/improvement.json'
config=json.loads(SOURCE.read_text())
rows=json.loads((ROOT/'Saved/ToonImprovement/character-faces.json').read_text())['characters']
rows=[r for r in rows if r['id'] in config['faces']]
assert len(rows)==len(config['faces'])
LIB=unreal.MaterialEditingLibrary
EAL=unreal.EditorAssetLibrary
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
validate='-PGToonValidate' in unreal.SystemLibrary.get_command_line()
report=dict(status='RUNNING',faces=[],comparison=config['comparison_map'])
packages=[r['source'].split('.')[0] for r in rows]+[config['comparison_map']]
if not validate:
    tx=Transaction(ROOT,RUN);tx.prepare(packages,SOURCE)
    for row in rows:
        original=unreal.load_asset(row['source']);candidate=unreal.load_asset(row['candidate'])
        tx.mark_written(row['source'].split('.')[0])
        LIB.set_material_instance_parent(original,candidate.parent)
        for name in LIB.get_scalar_parameter_names(candidate):
            if str(name).startswith('FaceSDF'):
                LIB.set_material_instance_scalar_parameter_value(original,name,LIB.get_material_instance_scalar_parameter_value(candidate,name))
        LIB.set_material_instance_texture_parameter_value(original,'FaceSDFTexture',unreal.load_asset(row['texture']))
        LIB.update_material_instance(original)
        assert EAL.save_loaded_asset(original,only_if_is_dirty=False)
    assert level.load_level(config['comparison_source'])
    models=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PGToonPreviewActor) and a.actor_has_tag('PGShadingComparisonModel')]
    for model in models:
        if model.actor_has_tag('PGShadingStage5') or model.actor_has_tag('PGShadingStage7'):
            model.hair_shadow_proxy.set_cast_shadow(False)
            model.hair_shadow_proxy.set_skeletal_mesh_asset(None)
            model.toon_presentation.hair_shadow_mesh=unreal.load_asset(config['hair_proxy'])
            model.toon_presentation.hair_shadow_distance=700
        if model.toon_presentation.key_light:
            key=model.toon_presentation.key_light
            key.set_editor_property('tags',list(set(map(str,key.tags))|{'PGToonKeyLight'}))
            key.light_component.set_editor_property('light_source_angle',config['light_source_angle_degrees'])
    tx.mark_written(config['comparison_map'])
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,config['comparison_map'])
    tx.data['status']='WRITTEN';tx.flush()
for row in rows:
    original=unreal.load_asset(row['source']);candidate=unreal.load_asset(row['candidate'])
    assert original.parent==candidate.parent,row['id']
    assert LIB.get_material_instance_texture_parameter_value(original,'FaceSDFTexture')==unreal.load_asset(row['texture'])
    assert LIB.get_material_instance_scalar_parameter_value(original,'FaceSDFEnabled')==1,row['id']
    report['faces'].append(dict(id=row['id'],status='PASS'))
assert level.load_level(config['comparison_map'])
models=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PGToonPreviewActor) and (a.actor_has_tag('PGShadingStage5') or a.actor_has_tag('PGShadingStage7'))]
assert len(models)==2
for model in models:
    assert model.toon_presentation.hair_shadow_mesh==unreal.load_asset(config['hair_proxy'])
    assert not model.hair_shadow_proxy.get_skeletal_mesh_asset()
report['status']='PASS'
write_json(RUN/('validate.json' if validate else 'apply.json'),report)

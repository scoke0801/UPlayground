"""Inspect the camera-fade sword's authored mask and instance inheritance."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
lib = unreal.MaterialEditingLibrary
mi = unreal.load_asset('/Game/ExternalAssets/LevelDesign/Dungeon_Pack/Assets/Pack_Characters/Clothing/Props/Materials/MI_Sword-Dagger')
material = mi.get_editor_property('parent')
report = dict(instance=mi.get_path_name(), parent=material.get_path_name(),
              blend=str(material.get_editor_property('blend_mode')), clip=material.get_editor_property('opacity_mask_clip_value'), nodes=[])
for node in lib.get_material_expressions(material):
    row = dict(name=node.get_name(), type=node.get_class().get_name(),
               outputs=[str(n) for n in lib.get_material_expression_output_names(node)],
               inputs=[n.get_name() if n else None for n in lib.get_inputs_for_material_expression(material, node)])
    for key in ['parameter_name', 'default_value', 'r', 'code', 'primitive_data_index']:
        try: row[key] = str(node.get_editor_property(key))
        except Exception: pass
    report['nodes'].append(row)
report['mask'] = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK).get_name()
report['originals'] = []
for path in ['/Game/ExternalAssets/LevelDesign/Dungeon_Pack/Assets/Materials/Master_Materials/M_Asset_Master_Material',
             '/Game/ExternalAssets/LevelDesign/Dungeon_Pack/Assets/Pack_Characters/Master_Materials/M_Character_Master_Material']:
    source = unreal.load_asset(path)
    node = lib.get_material_property_input_node(source, unreal.MaterialProperty.MP_OPACITY_MASK)
    report['originals'].append(dict(path=path, blend=str(source.get_editor_property('blend_mode')),
        node=node.get_name() if node else None, output=lib.get_material_property_input_node_output_name(source, unreal.MaterialProperty.MP_OPACITY_MASK)))
out = root/'Saved/QA/SwordFade'
out.mkdir(parents=True, exist_ok=True)
(out/'inspect.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

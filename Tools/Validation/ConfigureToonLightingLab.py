"""Create lit toon variants and a lighting lab. Original masters/maps remain intact."""
import hashlib
import json
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
import ConfigureToonCharacterTest as shared

DEST = '/Game/Art/ToonTest/Advanced'
OUT = ROOT / 'Saved/ToonTest/LightingLab' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'characters': []}
LATEST = ROOT / 'Saved/ToonTest/lighting_lab.json'
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def screen_outline():
    mat = shared.recreate_material('M_PGToonScreenOutline')
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)
    scene = shared.expression(mat, unreal.MaterialExpressionSceneTexture, -700, 0)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    # Explicit texture nodes declare depth/stencil resource dependencies to the compiler.
    depth = shared.expression(mat, unreal.MaterialExpressionSceneTexture, -700, 100)
    depth.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    stencil = shared.expression(mat, unreal.MaterialExpressionSceneTexture, -700, 200)
    stencil.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    inputs = [(scene, 'Color', 'SceneColor'), (depth, 'Color', 'DepthDependency'), (stencil, 'Color', 'StencilDependency')]
    for i, (key, value) in enumerate([('StencilID', 73), ('WidthPixels', 1.15), ('DepthBias', 1), ('Strength', 1),
                                    ('OverlapStrength', .65), ('OverlapDepthCm', 12), ('OverlapRelativeDepth', .015)]):
        inputs.append((shared.scalar_parameter(mat, key, value, -700, 300+i*100), '', key))
    inputs.append((shared.vector_parameter(mat, 'OutlineColor', (.018, .012, .026, 1), -700, 800), 'RGB', 'OutlineColor'))
    node = shared.expression(mat, unreal.MaterialExpressionCustom, -200, 0)
    node.set_editor_property('inputs', [shared.custom_input(pin) for _, _, pin in inputs])
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('code', (ROOT / 'Tools/Art/ToonTest/ToonScreenOutline.hlsl').read_text())
    for src, output, pin in inputs:
        assert LIB.connect_material_expressions(src, output, node, pin)
    assert LIB.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat)
    shared.save(mat)
    return mat


def main():
    folder = ROOT / 'Content/Art/ToonTest/Advanced'
    if folder.exists():
        shutil.copytree(folder, OUT / 'backup')
    shared.MASTER_DEST = DEST + '/Materials'
    mode_path = DEST + '/BP_PGToonLabGameMode'
    mode = unreal.load_asset(mode_path) if unreal.EditorAssetLibrary.does_asset_exist(mode_path) else None
    if not mode:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.GameModeBase)
        mode = TOOLS.create_asset('BP_PGToonLabGameMode', DEST, unreal.Blueprint, factory)
    mode_default = unreal.get_default_object(mode.generated_class())
    mode_default.set_editor_property('default_pawn_class', None)
    mode_default.set_editor_property('hud_class', None)
    unreal.BlueprintEditorLibrary.compile_blueprint(mode)
    shared.save(mode)
    masters = {}
    for key, extended, trans in [('inori', False, False), ('multi', True, False), ('transparent', True, True)]:
        masters[key] = shared.build_toon_master('M_PGToonWorld_' + key, extended, trans, world_lit=True)
    masters['overlay'] = shared.build_toon_master('M_PGToonWorld_overlay', False, True)
    outline = screen_outline()
    audit = json.loads((ROOT / 'Saved/ToonTest/additional_import.json').read_text())
    entries = [{'name': 'Inori', 'mesh': '/Game/Art/ToonTest/Inori/SK_Inori_ToonTest'}] + audit['characters']
    for entry in entries:
        mesh = unreal.load_asset(entry['mesh'])
        row = {'name': entry['name'], 'mesh': entry['mesh'], 'materials': []}
        for i, slot in enumerate(mesh.get_editor_property('materials')):
            name = str(slot.material_slot_name)
            source = slot.material_interface
            overlay = False
            if entry['name'] == 'Inori':
                overlays = json.loads((ROOT / 'Saved/ToonTest/outline.json').read_text())['overlays']
                if name in overlays:
                    source = unreal.load_asset(overlays[name])
                    overlay = True
            dst = DEST + '/' + entry['name'] + '/MI_PGWorld_' + source.get_name()
            if unreal.EditorAssetLibrary.does_asset_exist(dst):
                mi = unreal.load_asset(dst)
            else:
                mi = unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), dst)
            assert mi
            translucent = entry['name'] != 'Inori' and entry['slots'][i]['transparent']
            parent = masters['overlay' if overlay else 'transparent' if translucent else 'inori' if entry['name'] == 'Inori' else 'multi']
            LIB.set_material_instance_parent(mi, parent)
            profile = shared.apply_shading_profile(mi, entry['name'], name)
            face = profile['name'] in ['face', 'detail']
            for param, value in [('FaceShading', .85 if face else 0), ('HairAnisotropy', .8 if profile['name'] == 'hair' else 0),
                                 ('ShadowCast', 0. if face else 1.),
                                 ('WorldLightingInfluence', .55 if face else .75 if translucent else .88), ('ShadeStrength', .45 if face else .6)]:
                LIB.set_material_instance_scalar_parameter_value(mi, param, value)
            if profile['name'] == 'hair':
                LIB.set_material_instance_scalar_parameter_value(mi, 'SpecularStrength', profile['scalars']['SpecularStrength'])
                LIB.set_material_instance_scalar_parameter_value(mi, 'WorldLightingInfluence', shared.hair_world_lighting_influence())
            if overlay:
                LIB.set_material_instance_scalar_parameter_value(mi, 'ShadeStrength', 0)
                LIB.set_material_instance_scalar_parameter_value(mi, 'RimStrength', 0)
                LIB.set_material_instance_scalar_parameter_value(mi, 'SpecularStrength', 0)
                LIB.set_material_instance_vector_parameter_value(mi, 'LightTint', unreal.LinearColor(1, 1, 1, 1))
            LIB.update_material_instance(mi)
            shared.save(mi)
            row['materials'].append({'slot': name, 'asset': dst, 'profile': profile['name'], 'overlay': overlay})
        REPORT['characters'].append(row)
    # Also migrate already-generated fixtures so PIE does not spawn a visible default sphere pawn.
    for map_name in ['L_PGToon_LightingLab', 'L_PGToon_AdvancedMotionTest']:
        map_path = '/Game/Art/ToonTest/Maps/' + map_name
        map_file = ROOT / 'Content/Art/ToonTest/Maps' / (map_name+'.umap')
        if map_file.exists():
            shutil.copy2(map_file, OUT / (map_name+'.umap'))
            world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
            world.get_world_settings().set_editor_property('default_game_mode', mode.generated_class())
            assert unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
    REPORT.update(status='PASS', outline=outline.get_path_name(), game_mode=mode_path)


if __name__ == '__main__':
    try:
        main()
    except Exception:
        REPORT.update(status='FAIL', error=traceback.format_exc())
        unreal.log_error(REPORT['error'])
    finally:
        text = json.dumps(REPORT, ensure_ascii=False, indent=2)
        LATEST.write_text(text, encoding='utf-8')
        (OUT / 'configure.json').write_text(text, encoding='utf-8')
    if REPORT['status'] != 'PASS':
        raise RuntimeError('Lighting lab configuration failed: ' + str(LATEST))

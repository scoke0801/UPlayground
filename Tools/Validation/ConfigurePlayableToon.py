"""Bind model-specific SDF receivers and masked hair casters to selectable appearances."""
import json
import os
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterCatalog import PLAYER_IDS
from PlayableCharacterTransaction import Transaction, sha256, write_json

RUN = Path(os.environ['PG_PLAYABLE_TOON_RUN'])
DEST = '/Game/DataCenter/Characters/Toon'
VERIFY = '-PGPlayableToonVerify' in unreal.SystemLibrary.get_command_line()
LIB, EAL = unreal.MaterialEditingLibrary, unreal.EditorAssetLibrary
faces = {r['id']: r for r in json.loads((ROOT/'Tools/Art/ToonTest/CharacterFaces/bakes.json').read_text(encoding='utf-8'))['characters']}
hair_settings = json.loads((ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison/hair_shadow_settings.json').read_text())
sources = {
    'M_PGPlayableFaceSDFWorld': '/Game/Art/ToonTest/BokuseiFaceSDF/Materials/M_PGBokuseiFaceSDFWorld',
    'M_PGPlayableHairShadow': '/Game/Art/ToonTest/BokuseiShadingComparison/Materials/M_PGComparison_HairShadow',
    'M_PGPlayableNoShadow': '/Game/Art/ToonTest/BokuseiShadingComparison/Materials/M_PGComparison_NoShadow',
}


def own(name, parent, source=None):
    path = DEST+'/'+name
    tx.mark_written(path)
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    for kind in ['scalar', 'vector', 'texture']:
        material.set_editor_property(kind+'_parameter_values', [])
    LIB.set_material_instance_parent(material, parent)
    if source:
        for kind in ['scalar', 'vector', 'texture']:
            for key in getattr(LIB, 'get_'+kind+'_parameter_names')(source):
                value = getattr(LIB, 'get_material_instance_'+kind+'_parameter_value')(source, key)
                if value is not None:
                    getattr(LIB, 'set_material_instance_'+kind+'_parameter_value')(material, key, value)
    return material


rows, packages, protected = [], list(DEST+'/'+n for n in sources), {}
for identity in PLAYER_IDS:
    appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_'+identity)
    assert appearance and not appearance.parts, (identity, 'Expected single-mesh player')
    mesh = appearance.mesh
    assert mesh
    slots = list(mesh.get_editor_property('materials'))
    face_index = next(i for i,s in enumerate(slots) if str(s.material_slot_name) == 'Mat_Bokusei_Face') if identity == 'Bokusei' else faces[identity]['slot']['index']
    face = slots[face_index].material_interface
    assert LIB.get_material_instance_scalar_parameter_value(face, 'FaceSDFEnabled') > .5, identity
    assert LIB.get_material_instance_texture_parameter_value(face, 'FaceSDFTexture'), identity
    hair = [i for i,s in enumerate(slots) if 'hair' in str(s.material_slot_name).lower() or
            LIB.get_material_instance_scalar_parameter_value(s.material_interface, 'HairAnisotropy') > .5 or
            LIB.get_material_instance_scalar_parameter_value(s.material_interface, 'HairSoftness') > .01]
    assert hair and face_index not in hair, (identity, hair, face_index)
    packages += [appearance.get_path_name().split('.')[0], DEST+'/MI_PGFace_'+identity]
    packages += [DEST+'/MI_PGHair_'+identity+'_'+str(i) for i in hair]
    for asset in [mesh]+[s.material_interface for s in slots]:
        path = 'Content/'+asset.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset'
        protected[path] = sha256(ROOT/path)
    rows.append((identity, appearance, slots, face_index, hair))

if not VERIFY:
    tx = Transaction(ROOT, RUN)
    tx.prepare(packages, ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison/hair_shadow_settings.json')
    write_json(RUN/'protected.json', protected)
    for name, source in sources.items():
        target = DEST+'/'+name
        tx.mark_written(target)
        if not EAL.does_asset_exist(target):
            assert EAL.duplicate_asset(source, target)
        assert EAL.save_asset(target, only_if_is_dirty=False)
    face_master = unreal.load_asset(DEST+'/M_PGPlayableFaceSDFWorld')
    hair_master = unreal.load_asset(DEST+'/M_PGPlayableHairShadow')
    invisible = unreal.load_asset(DEST+'/M_PGPlayableNoShadow')
    for identity, appearance, slots, index, hair in rows:
        material = own('MI_PGFace_'+identity, face_master, slots[index].material_interface)
        for key,value in dict(FaceSDFEnabled=1., WorldLightingInfluence=.65, ShadowCast=0.).items():
            LIB.set_material_instance_scalar_parameter_value(material, key, value)
        LIB.update_material_instance(material)
        assert EAL.save_loaded_asset(material)
        overrides = dict(appearance.get_editor_property('material_overrides'))
        overrides[index] = material
        shadows = [invisible]*len(slots)
        for i in hair:
            caster = own('MI_PGHair_'+identity+'_'+str(i), hair_master, slots[i].material_interface)
            LIB.set_material_instance_scalar_parameter_value(caster, 'OpacityCutoff', hair_settings['opacity_cutoff'])
            LIB.set_material_instance_scalar_parameter_value(caster, 'ShadowInset', hair_settings['inset_cm'])
            LIB.update_material_instance(caster)
            assert EAL.save_loaded_asset(caster)
            shadows[i] = caster
        tx.mark_written(appearance.get_path_name().split('.')[0])
        appearance.set_editor_property('material_overrides', overrides)
        appearance.set_editor_property('hair_shadow_materials', shadows)
        appearance.set_editor_property('hair_shadow_mesh', None)
        # Covers the playable quarter-view zoom (up to 1600 cm) as well as portraits.
        appearance.set_editor_property('hair_shadow_distance', 1800.)
        assert EAL.save_loaded_asset(appearance)
    tx.data['status'] = 'WRITTEN'
    tx.flush()

report = dict(status='PASS', characters=[])
for identity, appearance, slots, index, hair in rows:
    overrides = appearance.get_editor_property('material_overrides')
    receiver = overrides[index]
    shadows = appearance.get_editor_property('hair_shadow_materials')
    assert receiver.get_path_name() == DEST+'/MI_PGFace_'+identity+'.MI_PGFace_'+identity
    assert receiver.get_editor_property('parent').get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    assert LIB.get_material_instance_scalar_parameter_value(receiver, 'FaceSDFEnabled') == 1.
    assert LIB.get_material_instance_texture_parameter_value(receiver, 'FaceSDFTexture') == LIB.get_material_instance_texture_parameter_value(slots[index].material_interface, 'FaceSDFTexture')
    assert len(shadows) == len(slots) and not appearance.hair_shadow_mesh
    for i, caster in enumerate(shadows):
        assert caster
        if i in hair:
            assert caster.get_editor_property('parent') == unreal.load_asset(DEST+'/M_PGPlayableHairShadow')
        else:
            assert caster == unreal.load_asset(DEST+'/M_PGPlayableNoShadow')
    report['characters'].append(dict(id=identity, face_slot=index, hair_slots=hair, slots=len(slots)))
for path,digest in json.loads((RUN/'protected.json').read_text()).items():
    assert sha256(ROOT/path) == digest, path
write_json(RUN/('verify.json' if VERIFY else 'configure.json'), report)
unreal.log('PGPlayableToon assets PASS players='+str(len(rows)))

"""Backed-up particle integration. External source packages and gameplay stay unchanged."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayerNiagaraData import profiles, snapshot

DEST = '/Game/Art/PlayerCombatFX/External'
SLASH_MATERIAL_SOURCE = '/Game/ExternalAssets/VFX/MixedVFX/Materials/MaterialInstances/MI_Slash_01'
SOURCES = {
    'Slash': '/Game/ExternalAssets/VFX/MixedVFX/Particles/Slashes/SeparateParts/Slashes/NS_HolySlash_OnlySlash',
    'Thrust': '/Game/ExternalAssets/VFX/SlashTrail_SoftTofu/Niagara/Basic/NS_Hit_Basic_Once',
    'Impact': '/Game/ExternalAssets/VFX/Niagara/GroundRocks/NS_GroundBurstRocks',
}
SPEC = {
    'CRESCENT': dict(system='Slash', scale=(1,1,1), rotation=(0,0,0), offset=(0,0,0), intensity=.45, duration=.24, mirror=True),
    'CLEAVE': dict(system='Slash', scale=(1,1,1), rotation=(0,0,58), offset=(0,0,0), intensity=.45, duration=.24, mirror=True),
    'ORBIT': dict(system='Slash', scale=(1,1,1), rotation=(0,0,0), offset=(0,0,0), intensity=.35, duration=.24, mirror=True),
    'THRUST': dict(system='Thrust', scale=(.65,.65,.65), rotation=(0,0,0), offset=(.6,0,0), intensity=.7, duration=.24, mirror=False),
    'IMPACT': dict(system='Impact', scale=(.25,.25,.16), rotation=(0,0,0), offset=(0,0,0), intensity=.5, duration=.35, mirror=False),
    'BLADE': dict(system='Slash', scale=(.65,1,.65), rotation=(0,0,0), offset=(0,0,0), intensity=.4, duration=.24, mirror=False),
}
assets = profiles()
validate = 'PGExternalVFXValidate' in unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())[1]
pointer = ROOT/'Saved/ExternalCombatVFX_LastBackup.txt'

def file_for(path):
    return ROOT/'Content'/(path.split('.')[0].removeprefix('/Game/')+'.uasset')

def hashes():
    return {name: hashlib.sha256(file_for(path).read_bytes()).hexdigest() for name,path in dict(SOURCES, SlashMaterial=SLASH_MATERIAL_SOURCE).items()}

def state():
    return {str(k): dict(snapshot(v), shapes=[s.name for s in v.get_editor_property('swing_shapes')],
                         authored=v.get_editor_property('use_authored_vfx'),
                         build=v.get_editor_property('build_reactive_vfx')) for k,v in assets.items()}

def check():
    slash_material = unreal.load_asset(DEST+'/MI_PGExternalSlash')
    overrides = slash_material.get_editor_property('base_property_overrides')
    assert overrides.get_editor_property('override_blend_mode')
    assert overrides.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_ADDITIVE
    for skill, profile in assets.items():
        mapping = profile.get_editor_property('external_vfx')
        required = set(profile.get_editor_property('swing_shapes'))
        if skill == 114: required.add(unreal.PGPlayerVFXShape.BLADE)
        assert set(mapping) == required, skill
        for shape, definition in mapping.items():
            spec = SPEC[shape.name]
            assert definition.get_editor_property('system').get_path_name().split('.')[0] == DEST+'/NS_PGExternal'+spec['system']
            assert abs(definition.get_editor_property('reference_duration')-spec['duration']) < .0001
            assert abs(definition.get_editor_property('intensity')-spec['intensity']) < .0001
            assert abs(definition.get_editor_property('reference_radius')-200.) < .0001
            assert definition.get_editor_property('mirror') == spec['mirror']
            for field in ('scale', 'offset'):
                value = definition.get_editor_property(field)
                assert all(abs(a-b)<.0001 for a,b in zip((value.x,value.y,value.z),spec[field])), (skill,shape,field)
            rotation = definition.get_editor_property('rotation')
            assert all(abs(a-b)<.0001 for a,b in zip((rotation.pitch,rotation.yaw,rotation.roll),spec['rotation']))
        assert profile.get_editor_property('use_authored_vfx') and profile.get_editor_property('build_reactive_vfx'), skill
    for kind in SOURCES:
        system = unreal.load_asset(DEST+'/NS_PGExternal'+kind)
        description = json.loads(unreal.PGNiagaraFXTools.describe_system(system))
        assert description['compiled'], kind
        assert all(e['local_space'] for e in description['emitters'] if e['enabled']), kind
        if kind == 'Slash':
            assert all(not e['enabled'] for e in description['emitters'] if e['name'] in ('Smoke','Feather'))
            assert any('MI_PGExternalSlash' in r for e in description['emitters'] for r in e['renderers'])
        if kind == 'Impact':
            assert any('M_PGImpactDebris' in r for e in description['emitters'] for r in e['renderers'])

if validate:
    backup = Path(pointer.read_text())
    assert state() == json.loads((backup/'before.json').read_text()), 'Existing presentation/gameplay changed'
    assert hashes() == json.loads((backup/'sources.json').read_text()), 'External source changed'
    check()
    unreal.log('PGExternalVFX VALIDATE PASS profiles=8 systems=3 gameplay_unchanged=1 sources_unchanged=1')
else:
    backup = ROOT/'Saved/Backups/ExternalCombatVFX'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup.mkdir(parents=True)
    before = state()
    (backup/'before.json').write_text(json.dumps(before, indent=2))
    (backup/'sources.json').write_text(json.dumps(hashes(), indent=2))
    for path in [a.get_path_name() for a in assets.values()] + [DEST+'/NS_PGExternal'+k for k in SOURCES] + [DEST+'/M_PGImpactDebris',DEST+'/MI_PGExternalSlash']:
        source = file_for(path)
        if source.exists():
            target = backup/source.relative_to(ROOT/'Content')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    material = unreal.load_asset(DEST+'/M_PGImpactDebris') if unreal.EditorAssetLibrary.does_asset_exist(DEST+'/M_PGImpactDebris') else None
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_PGImpactDebris', DEST, unreal.Material, unreal.MaterialFactoryNew())
        lib = unreal.MaterialEditingLibrary
        color = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant', unreal.LinearColor(.075,.09,.11,1))
        lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        roughness = lib.create_material_expression(material, unreal.MaterialExpressionConstant)
        roughness.set_editor_property('r', .9)
        lib.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
        material.set_editor_property('used_with_niagara_mesh_particles', True)
        lib.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    slash_material_path = DEST+'/MI_PGExternalSlash'
    slash_material = unreal.load_asset(slash_material_path) if unreal.EditorAssetLibrary.does_asset_exist(slash_material_path) else unreal.EditorAssetLibrary.duplicate_asset(SLASH_MATERIAL_SOURCE, slash_material_path)
    overrides = slash_material.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_blend_mode', True)
    overrides.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    slash_material.set_editor_property('base_property_overrides', overrides)
    unreal.MaterialEditingLibrary.update_material_instance(slash_material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(slash_material, only_if_is_dirty=False)
    systems = {}
    for kind, source in SOURCES.items():
        path = DEST+'/NS_PGExternal'+kind
        system = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(source, path)
        assert system, path
        if kind == 'Slash':
            assert unreal.PGNiagaraFXTools.configure_slash(system)
            assert unreal.PGNiagaraFXTools.configure_combat_burst(system, False, slash_material)
        else: assert unreal.PGNiagaraFXTools.configure_combat_burst(system, kind == 'Thrust', material if kind == 'Impact' else None)
        assert unreal.EditorAssetLibrary.save_loaded_asset(system, only_if_is_dirty=False)
        systems[kind] = system
        (backup/(kind+'.json')).write_text(unreal.PGNiagaraFXTools.describe_system(system), encoding='utf-8')
    for skill, profile in assets.items():
        shapes = set(profile.get_editor_property('swing_shapes'))
        if skill == 114: shapes.add(unreal.PGPlayerVFXShape.BLADE)
        mapping = {}
        for shape in shapes:
            spec = SPEC[shape.name]
            entry = unreal.PGExternalCombatVFX()
            entry.set_editor_property('system', systems[spec['system']])
            entry.set_editor_property('reference_radius', 200.)
            entry.set_editor_property('reference_duration', spec['duration'])
            entry.set_editor_property('scale', unreal.Vector(*spec['scale']))
            entry.set_editor_property('offset', unreal.Vector(*spec['offset']))
            pitch, yaw, roll = spec['rotation']
            entry.set_editor_property('rotation', unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
            entry.set_editor_property('intensity', spec['intensity'])
            entry.set_editor_property('mirror', spec['mirror'])
            mapping[shape] = entry
        profile.set_editor_property('external_vfx', mapping)
        assert unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    assert state() == before
    assert hashes() == json.loads((backup/'sources.json').read_text())
    check()
    pointer.write_text(str(backup))
    unreal.log('PGExternalVFX APPLY PASS profiles=8 systems=3 backup='+str(backup))

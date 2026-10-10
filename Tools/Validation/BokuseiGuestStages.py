"""Independent, cumulative comparison materials for each guest identity."""
import json
from pathlib import Path
import unreal

DEST = '/Game/Art/ToonTest/BokuseiShadingComparison'
STAGE_IDS = ['Lit', 'Cel', 'Parts', 'Rim', 'Toon', 'Shadow', 'FaceSDF', 'FaceSDFWorld']
LIB = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
_MASTERS = None


def build_stages(identity, mesh):
    global _MASTERS
    import ConfigureToonCharacterTest as shared
    from ConfigureBokuseiShadingComparison import own, save, toon_variant
    root = Path(unreal.Paths.project_dir()).resolve()
    faces = json.loads((root/'Tools/Art/ToonTest/CharacterFaces/bakes.json').read_text(encoding='utf-8'))
    face = next(row for row in faces['characters'] if row['id'] == identity)
    candidate = unreal.load_asset('/Game/Art/ToonTest/Improvement/Faces/MI_PGFaceSDF_'+identity)
    assert candidate, identity
    texture = LIB.get_material_instance_texture_parameter_value(candidate, 'FaceSDFTexture')
    assert texture, identity
    # Never inherit a live face parent: it may already contain SDF or world lighting.
    old_dest = shared.MASTER_DEST
    shared.MASTER_DEST = DEST+'/Materials'
    try:
        if _MASTERS is None:
            _MASTERS = {}
            for lit in [False, True]:
                for transparent in [False, True]:
                    name = 'M_PGGuest_'+('World' if lit else 'Toon')+('Transparent' if transparent else '')
                    _MASTERS[lit, transparent] = shared.build_toon_master(name, True, transparent, lit, texture)
        masters = _MASTERS
    finally:
        shared.MASTER_DEST = old_dest
    default = [unreal.load_asset(DEST+'/Materials/M_PGComparison_DefaultLit'+suffix) for suffix in ['', 'Transparent']]
    assert all(default)

    def instance(name, parent, source, scalars=None):
        mi = own('MI_PGGuest_'+identity+'_'+name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        for kind in ['scalar', 'vector', 'texture']:
            mi.set_editor_property(kind+'_parameter_values', [])
        LIB.set_material_instance_parent(mi, parent)
        for kind in ['scalar', 'vector', 'texture']:
            for key in getattr(LIB, 'get_'+kind+'_parameter_names')(source):
                value = getattr(LIB, 'get_material_instance_'+kind+'_parameter_value')(source, key)
                if value is not None:
                    getattr(LIB, 'set_material_instance_'+kind+'_parameter_value')(mi, key, value)
        for key, value in (scalars or {}).items():
            LIB.set_material_instance_scalar_parameter_value(mi, key, value)
        LIB.update_material_instance(mi)
        save(mi)
        return mi

    stages = [[] for _ in STAGE_IDS]
    slots = list(mesh.get_editor_property('materials'))
    for index, slot in enumerate(slots):
        source = slot.material_interface
        parent = source
        while isinstance(parent, unreal.MaterialInstanceConstant):
            parent = parent.get_editor_property('parent')
        transparent = parent.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
        baseline = dict(FaceSDFEnabled=0., FaceShading=0., HairAnisotropy=0.)
        toon = instance(str(index)+'_Toon', masters[False, transparent], source, baseline)
        hair = LIB.get_material_instance_scalar_parameter_value(source, 'HairSoftness') > 0
        world = instance(str(index)+'_World', masters[True, transparent], source,
                         dict(baseline, WorldLightingInfluence=shared.hair_world_lighting_influence() if hair else .65))
        materials = [instance(str(index)+'_Lit', default[int(transparent)], source),
                     toon_variant(toon, 'Guest_'+identity+'_'+str(index), True),
                     toon_variant(toon, 'Guest_'+identity+'_'+str(index)), toon, toon, world, toon, world]
        if index == face['slot']['index']:
            for stage, lit in [(6, False), (7, True)]:
                materials[stage] = instance(str(index)+'_'+STAGE_IDS[stage], masters[lit, transparent], candidate,
                                           dict(FaceSDFEnabled=1., WorldLightingInfluence=.65, HairAnisotropy=0.))
        for stage, material in zip(stages, materials):
            stage.append(material)

    # Source slot names may be hashes. The authored anisotropy flag identifies
    # hair for those imports; readable hair slot names remain supported.
    invisible = unreal.load_asset(DEST+'/Materials/M_PGComparison_NoShadow')
    hair_master = unreal.load_asset(DEST+'/Materials/M_PGComparison_HairShadow')
    assert invisible and hair_master
    hair = []
    hair_count = 0
    for index, slot in enumerate(slots):
        source = slot.material_interface
        is_hair = 'hair' in str(slot.material_slot_name).lower() or LIB.get_material_instance_scalar_parameter_value(source, 'HairAnisotropy') > .5
        hair.append(instance(str(index)+'_Hair', hair_master, source, dict(OpacityCutoff=.45)) if is_hair else invisible)
        hair_count += int(is_hair)
    assert hair_count, 'No authored hair slots: '+identity
    return stages, hair

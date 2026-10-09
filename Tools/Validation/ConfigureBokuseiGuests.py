"""Add guests to the existing saved map, preserving the eight authored stages."""
import json
import shutil
import sys
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from BokuseiGuestModels import add_guests, validate_guests

from ConfigureBokuseiShadingComparison import MAP, RUN, label_material
report = dict(status='RUNNING', run=str(RUN), map=MAP)
try:
    map_file = ROOT/'Content/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison.umap'
    shutil.copy2(map_file, RUN/'previous_map.umap')
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    key = next(a.toon_presentation.key_light for a in actors.get_all_level_actors() if a.actor_has_tag('PGShadingStage1'))

    def prop(name, shape, location, scale, material, rotation=unreal.Rotator()):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), rotation)
        actor.set_actor_label(name)
        c = actor.static_mesh_component
        c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+shape))
        c.set_material(0, material)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    report['guests'] = add_guests(actors, key, prop, label_material)
    report['validated'] = validate_guests(actors.get_all_level_actors())
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    report['status'] = 'PASS'
except Exception:
    report.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(report['error'])
finally:
    payload = json.dumps(report, ensure_ascii=False, indent=2)
    (ROOT/'Saved/BokuseiShadingComparison/guests.json').write_text(payload, encoding='utf-8')
    (RUN/'guests.json').write_text(payload, encoding='utf-8')
if report['status'] != 'PASS':
    raise RuntimeError(report['error'])

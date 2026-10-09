"""Reload saved assets in a fresh UE process; verify placement and ownership."""
import hashlib
import json
from pathlib import Path
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/ForestRuins'
KIT=ROOT/'Tools/Art/ForestRuins'
BUILD=json.loads((OUT/'build.json').read_text(encoding='utf-8'))
CONFIG=json.loads((KIT/'layout.json').read_text(encoding='utf-8'))
SOURCES=json.loads((KIT/'sources.json').read_text(encoding='utf-8'))
REPORT=dict(status='RUNNING',map=BUILD['map'],checks=[])
try:
    assert BUILD['status']=='PASS'
    for relative,expected in BUILD['protected'].items():
        assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
    for entry in SOURCES['meshes']+SOURCES['textures']:
        assert hashlib.sha256(Path(entry['source']).read_bytes()).hexdigest()==entry['sha256'],entry['source']
        assert hashlib.sha256((KIT/entry['file']).read_bytes()).hexdigest()==entry['sha256'],entry['file']
    REPORT['checks'].append('Original sources and existing gameplay files unchanged')
    detailed=json.loads((KIT/'detailed_sources.json').read_text(encoding='utf-8'))
    for entry in detailed['meshes']+list(detailed['textures'].values()):
        assert hashlib.sha256((KIT/entry['file']).read_bytes()).hexdigest()==entry['sha256'],entry['file']
        source=Path(detailed['source_root'])/Path(entry['file']).relative_to('DetailedSource')
        assert hashlib.sha256(source.read_bytes()).hexdigest()==entry['sha256'],str(source)
    REPORT['checks'].append('Detailed source meshes and PBR texture hashes preserved')
    world=unreal.EditorLoadingAndSavingUtils.load_map(CONFIG['map'])
    assert world
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    assert world.get_world_settings().get_editor_property('default_game_mode').get_path_name()==CONFIG['game_mode']
    starts=[a for a in actors if isinstance(a,unreal.PlayerStart)]
    assert len(starts)==1
    assert unreal.MathLibrary.vector_distance(starts[0].get_actor_location(),unreal.Vector(*CONFIG['player_start_cm']))<.01
    volumes=[a for a in actors if isinstance(a,unreal.NavMeshBoundsVolume)]
    navs=[a for a in actors if isinstance(a,unreal.RecastNavMesh)]
    assert len(volumes)==1 and len(navs)==1,(len(volumes),len(navs))
    assert navs[0].get_editor_property('runtime_generation')==unreal.RuntimeGenerationType.DYNAMIC
    REPORT['checks'].append('Stage GameMode, player start and dynamic navigation reloaded')
    instances={}
    colliders=[]
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh=comp.get_editor_property('static_mesh')
            if not mesh: continue
            assert mesh.get_path_name().startswith('/Game/Environment/ForestRuins/'),mesh.get_path_name()
            assert all(comp.get_material(i) for i in range(comp.get_num_materials())),mesh.get_path_name()
            if mesh.get_name().startswith('SM_PGFR_HD_'):
                row=BUILD['detailed_meshes'][mesh.get_name().removeprefix('SM_PGFR_')]
                assert mesh.get_num_triangles(0)==row['triangles'],mesh.get_name()
                assert all('/Materials/HD_' in comp.get_material(i).get_path_name() for i in range(comp.get_num_materials())),mesh.get_name()
            if isinstance(comp,unreal.HierarchicalInstancedStaticMeshComponent):
                name=mesh.get_name().removeprefix('SM_PGFR_')
                instances[name]=instances.get(name,0)+comp.get_instance_count()
                assert comp.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
                assert comp.get_editor_property('mobility')==unreal.ComponentMobility.STATIC
            else:
                assert comp.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS,actor.get_actor_label()
                colliders.append(actor.get_actor_label())
    assert instances==BUILD['instances'],('Saved instance placement changed',instances,BUILD['instances'])
    assert sum(instances.values())>600
    assert len(colliders)==6,(len(colliders),colliders)
    REPORT.update(instanced_mesh_groups=len(instances),total_instances=sum(instances.values()),collision_actors=colliders,actor_count=len(actors))
    REPORT['checks'].append('All resource references, HISM instance transforms and collisions persisted')
    REPORT['status']='PASS';unreal.log('PGForestRuins RELOAD PASS')
except BaseException as error:
    REPORT.update(status='FAIL',error=str(error),traceback=traceback.format_exc());unreal.log_error(REPORT['traceback']);raise
finally:
    (OUT/'validate.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')

"""Reload and exercise guest comparison controls independently of legacy material baselines."""
import json
import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from BokuseiGuestModels import validate_guests
OUT = ROOT/'Saved/BokuseiShadingComparison'
RUN = OUT/'GuestPreview'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
RUN.mkdir(parents=True)
report = dict(status='RUNNING', run=str(RUN), checks=[], images=[])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison')
assert world
report['reload'] = validate_guests(actors.get_all_level_actors())
cases = []
for identity in ['Arin', 'Hwarin', 'LianLian']:
    cases.append(('Tab', identity, 0, 'overview'))
    cases += [(key, identity, i, 'front') for i, key in enumerate(['One','Two','Three','Four','Five','Six','Seven','Eight'])]
    cases += [('F', identity, 7, 'face'), ('Seven', identity, 6, 'face'), ('C', identity, 6, 'quarter'),
              ('Eight', identity, 7, 'quarter'), ('H', identity, 7, 'shadow_on'), ('H', identity, 7, 'shadow_off'),
              ('J', identity, 7, 'hair_off'), ('J', identity, 7, 'hair_on'), ('R', identity, 0, 'overview')]
cases += [('Tab','Bokusei',0,'overview'), ('Eight','Bokusei',7,'front'), ('F','Bokusei',7,'face')]
cases += [('Tab',identity,7,'face') for identity in ['Arin','Hwarin','LianLian','Bokusei']]
cases += [('C','Bokusei',7,'quarter')]
cases += [('Tab',identity,7,'quarter') for identity in ['Arin','Hwarin','LianLian','Bokusei']]
cases += [('R','Bokusei',0,'overview')]
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
previous_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started = time.monotonic()
sent = None
index = 0
ready = False
ending = False
before = None


def finish(error=None):
    global ending
    report.update(status='FAIL' if error else 'PASS')
    if error: report['error'] = error
    payload = json.dumps(report, ensure_ascii=False, indent=2)
    (RUN/'guests-preview.json').write_text(payload, encoding='utf-8')
    (OUT/'guests-preview.json').write_text(payload, encoding='utf-8')
    performance.set_editor_property('bThrottleCPUWhenNotForeground', previous_throttle)
    ending = True
    level.editor_request_end_play()


def tick(_dt):
    global ready, sent, index, before
    try:
        game = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if ending:
            if not game:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            return
        now = time.monotonic()
        assert now-started < 240, 'PIE timeout'
        if not game or now-started < 5: return
        pawn = unreal.GameplayStatics.get_player_controller(game, 0).get_controlled_pawn()
        assert isinstance(pawn, unreal.PGShadingComparisonPawn)
        models = list(unreal.GameplayStatics.get_all_actors_of_class(game, unreal.SkeletalMeshActor))
        def model(identity, stage):
            tag = 'PGShadingStage'+str(stage) if identity == 'Bokusei' else 'PGShadingGuestStage_'+identity+'_'+str(stage)
            return next(a for a in models if a.actor_has_tag(tag))
        if not ready:
            assert unreal.PGToonPreviewActor.set_preview_viewport_size(game, 1280, 720)
            # First check each rig's synchronized animation before freezing it for A/B captures.
            for identity in ['Arin','Hwarin','LianLian']:
                leader = model(identity, 0).skeletal_mesh_component
                head = model(identity, 0).toon_presentation.head_bone
                reference = leader.get_socket_location(head)-model(identity, 0).get_actor_location()
                assert 90 < reference.z < 220, (identity, reference)
                for stage in range(1,8):
                    actor = model(identity, stage)
                    assert (actor.skeletal_mesh_component.get_socket_location(head)-actor.get_actor_location()-reference).length() < .01
                data = leader.get_editor_property('animation_data')
                leader.override_animation_data(data.anim_to_play, True, False, 1.25, 0)
            ready = True
            return
        if index == len(cases):
            finish()
            return
        key_name, identity, stage, view = cases[index]
        if sent is None:
            before = pawn.get_actor_location()
            key = unreal.Key()
            key.set_editor_property('key_name', key_name)
            pawn.send_probe_input(key, True, 0)
            pawn.send_probe_input(key, False, 0)
            sent = now
            return
        if now-sent < .65: return
        actor = model(identity, stage)
        base = actor.get_actor_location()
        if view == 'overview':
            overview = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.CameraActor) if a.actor_has_tag('PGShadingOverview'))
            expected = overview.get_actor_location()+base-model('Bokusei',0).get_actor_location()
            if identity != 'Bokusei': expected += unreal.Vector(0,250,0)
        elif view == 'face':
            expected = base+unreal.Vector(0,135,144) if identity == 'Bokusei' else actor.skeletal_mesh_component.get_socket_location(actor.toon_presentation.head_bone)+unreal.Vector(0,135,5)
        elif view in ['front','quarter']:
            expected = base+(unreal.Vector(0,360,125) if view == 'front' else unreal.Vector(230,390,330))
        else:
            expected = before
            if view.startswith('hair'):
                proxies = [a.hair_shadow_proxy for a in models if isinstance(a,unreal.PGToonPreviewActor) and a.hair_shadow_proxy.get_skeletal_mesh_asset()]
                assert len(proxies) == 8
                assert all(p.get_editor_property('cast_shadow') == (view == 'hair_on') for p in proxies)
            else:
                casters = [a.static_mesh_component for a in unreal.GameplayStatics.get_all_actors_of_class(game,unreal.StaticMeshActor) if a.actor_has_tag('PGShadingGuestShadowCaster')]
                assert len(casters) == 12
                assert all(c.get_editor_property('cast_shadow') == (view == 'shadow_on') for c in casters)
        assert (pawn.get_actor_location()-expected).length() < (1. if view == 'face' else .05), (identity,stage,view,pawn.get_actor_location(),expected)
        path = RUN/(str(index).zfill(2)+'_'+identity+'_'+str(stage+1)+'_'+view+'.png')
        assert unreal.PGEditorProbeTools.capture_game_viewport(game,str(path))
        report['images'].append(str(path))
        report['checks'].append(dict(key=key_name,model=identity,stage=stage+1,view=view,status='PASS'))
        sent = None
        index += 1
    except BaseException:
        finish(traceback.format_exc())


level.editor_request_begin_play()
handle = unreal.register_slate_post_tick_callback(tick)

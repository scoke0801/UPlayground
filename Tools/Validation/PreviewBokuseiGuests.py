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
before_rotation = None
previous_identity = 'Bokusei'
feature_probe = None
feature_wait_until = 0


def check_new_controls(game, pawn, model):
    def press(name, down=True):
        key = unreal.Key()
        key.set_editor_property('key_name', name)
        pawn.send_probe_input(key, down, 0)
    def tap(name):
        press(name)
        press(name, False)
    def record(name):
        report['checks'].append(dict(feature=name, status='PASS'))
    def capture(name):
        path = RUN/(name+'.png')
        assert unreal.PGEditorProbeTools.capture_game_viewport(game, str(path))
        report['images'].append(str(path))

    tap('Tilde')
    yield .2
    assert not pawn.is_help_visible()
    capture('HelpHidden')
    tap('Tilde')
    yield .2
    assert pawn.is_help_visible()
    capture('HelpRestored')
    record('help_toggle_slate_console_key')

    # Check every stage in order at the shipped timing, including smooth travel.
    tap('P')
    yield .1
    assert pawn.is_stage_sequence_playing()
    first = model('Bokusei', 0).get_actor_location()+unreal.Vector(0,360,125)
    assert (pawn.get_actor_location()-first).length() < .05
    visited = set()
    previous_x = pawn.get_actor_location().x
    deadline = time.monotonic()+35
    while pawn.is_stage_sequence_playing():
        location = pawn.get_actor_location()
        assert location.x >= previous_x-.01, 'Sequence moved backwards'
        assert abs(location.y-first.y) < .01 and abs(location.z-first.z) < .01
        previous_x = location.x
        for stage in range(8):
            if abs(location.x-model('Bokusei', stage).get_actor_location().x) < .05:
                visited.add(stage)
        assert time.monotonic() < deadline
        yield .03
    assert (pawn.get_actor_location()-(model('Bokusei',7).get_actor_location()+unreal.Vector(0,360,125))).length() < .05
    visited.add(7)
    assert visited == set(range(8)), visited
    capture('SequenceCompleted')
    record('sequence_all_eight_stages_default_timing')

    # Stop/restart and every manual view input must give camera control back.
    for name in ['P','W','RightMouseButton','Three','F','C','R','Zero']:
        tap('P')
        yield .15
        assert pawn.is_stage_sequence_playing()
        press(name)
        yield .15
        press(name, False)
        assert not pawn.is_stage_sequence_playing(), name
        record('sequence_cancel_'+name)
    tap('R')
    yield .1
    pawn.stage_hold_seconds = .15
    pawn.stage_move_seconds = .25
    for identity in ['Arin','Hwarin','LianLian','Bokusei']:
        tap('P')
        yield .25
        assert pawn.is_stage_sequence_playing()
        tap('Tab')
        yield .1
        assert pawn.is_stage_sequence_playing(), 'Model switch stopped sequence'
        deadline = time.monotonic()+10
        while pawn.is_stage_sequence_playing():
            assert time.monotonic() < deadline
            yield .05
        expected = model(identity,7).get_actor_location()+unreal.Vector(0,360,125)
        assert (pawn.get_actor_location()-expected).length() < .05, (identity, pawn.get_actor_location(), expected)
        record('sequence_model_switch_'+identity)
    pawn.stage_hold_seconds = 2
    pawn.stage_move_seconds = 1


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
    global ready, sent, index, before, before_rotation, previous_identity, feature_probe, feature_wait_until
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
            if feature_probe is None: feature_probe = check_new_controls(game, pawn, model)
            if now < feature_wait_until: return
            try:
                feature_wait_until = now+next(feature_probe)
            except StopIteration:
                finish()
            return
        key_name, identity, stage, view = cases[index]
        if sent is None:
            if key_name == 'Tab':
                # A deliberately non-preset framing must survive model switches.
                pawn.set_actor_location(pawn.get_actor_location()+unreal.Vector(37,-21,19), False, False)
                pc = unreal.GameplayStatics.get_player_controller(game, 0)
                rotation = pc.get_control_rotation()
                pc.set_control_rotation(unreal.Rotator(pitch=(rotation.pitch+180)%360-180+3,
                                                       yaw=(rotation.yaw+180)%360-180+7, roll=0))
            before = pawn.get_actor_location()
            before_rotation = unreal.GameplayStatics.get_player_controller(game,0).get_control_rotation()
            key = unreal.Key()
            key.set_editor_property('key_name', key_name)
            pawn.send_probe_input(key, True, 0)
            pawn.send_probe_input(key, False, 0)
            sent = now
            return
        if now-sent < .65: return
        actor = model(identity, stage)
        base = actor.get_actor_location()
        if key_name == 'Tab':
            expected = before+base-model(previous_identity,stage).get_actor_location()
            rotation = unreal.GameplayStatics.get_player_controller(game,0).get_control_rotation()
            assert (unreal.MathLibrary.get_forward_vector(rotation)-unreal.MathLibrary.get_forward_vector(before_rotation)).length() < .0001, (identity, view, before_rotation, rotation)
        elif view == 'overview':
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
        previous_identity = identity
        sent = None
        index += 1
    except BaseException:
        finish(traceback.format_exc())


level.editor_request_begin_play()
handle = unreal.register_slate_post_tick_callback(tick)

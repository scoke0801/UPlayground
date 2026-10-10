"""P2 integration: real PCG lifecycle, atomic branch loot, camera occlusion and room kit."""
import json
import os
import struct
import time
import traceback
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
OUT=Path(os.environ.get('PG_DUNGEON_QA_DIR',str(ROOT/'Saved/QA/ProceduralDungeon/P2/integration')))
OUT.mkdir(parents=True,exist_ok=True)
RENDER='-PGDungeonP2Render' in u.SystemLibrary.get_command_line()
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert L.load_level('/Game/Maps/L_PG_ProceduralDungeon')
editor_world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
editor_generator=u.GameplayStatics.get_actor_of_class(editor_world,u.PGDungeonGenerator)
editor_definition=editor_generator.get_editor_property('definition')
editor_graph=editor_definition.get_editor_property('decoration_graph')
try:
    editor_definition.set_editor_property('decoration_graph',None)
    assert not editor_generator.generate_preview()
    assert editor_generator.get_editor_property('state')==u.PGDungeonState.FAILED
    assert not editor_generator.get_components_by_class(u.InstancedStaticMeshComponent)
finally:
    editor_definition.set_editor_property('decoration_graph',editor_graph)
u.EditorPythonScripting.set_keep_python_script_alive(True)
assert u.PGEditorProbeTools.begin_play_window(1600,900)
start=time.monotonic(); at=start; stopped=None; phase='ready'
report=dict(status='RUNNING',images=[],direct_play=False)
target=None; original_graph=None; original_density=None
telegraph=None

def command(world,text): u.SystemLibrary.execute_console_command(world,text)
def reward_signature(world):
    result=[]
    for cache in u.GameplayStatics.get_all_actors_of_class(world,u.PGDungeonTreasure):
        item=cache.get_item()
        result.append((cache.get_editor_property('dungeon_room_id'),item.definition_id,
                       sorted((str(k),v) for k,v in item.options.items())))
    return sorted(result)
def boss_center(g):
    room=next(r for r in g.get_editor_property('layout').rooms if r.role==u.PGDungeonRoomRole.BOSS)
    d=g.get_editor_property('definition'); spacing=d.get_editor_property('room_size')+d.get_editor_property('corridor_length')
    return g.get_actor_location()+u.Vector(room.cell.x*spacing,room.cell.y*spacing,0)
def outside_boss(g):
    gate=next(c for c in g.get_components_by_class(u.StaticMeshComponent) if 'PGDungeonGate' in [str(t) for t in c.component_tags])
    position=gate.get_world_location(); direction=position-boss_center(g); direction.z=0
    length=(direction.x*direction.x+direction.y*direction.y)**.5
    return u.Vector(position.x+direction.x/length*700,position.y+direction.y/length*700,g.get_actor_location().z+100)
def capture(world,name):
    if RENDER:
        path=OUT/(name+'.png'); assert u.PGEditorProbeTools.capture_game_viewport(world,str(path))
        report['images'].append(str(path))
        report.setdefault('image_sizes',{})[name]=struct.unpack('>II',path.read_bytes()[16:24])
def finish(error=None):
    global stopped,target
    target=None
    report['status']='FAIL' if error else 'PASS'
    if error: report['error']=error; u.log_error(error)
    else: u.log('PGDungeonP2 PROBE PASS')
    (OUT/'probe.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    L.editor_request_end_play(); stopped=time.monotonic()

def tick(dt):
    global phase,at,target,original_graph,original_density,telegraph
    now=time.monotonic()
    if stopped:
        if now-stopped>4 and not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
            u.unregister_slate_post_tick_callback(handle); u.SystemLibrary.quit_editor()
        return
    try:
        assert now-start<400,phase
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world: return
        g=u.GameplayStatics.get_actor_of_class(world,u.PGDungeonGenerator)
        player=u.GameplayStatics.get_player_pawn(world,0)
        if not g or not player: return
        for enemy in u.GameplayStatics.get_all_actors_of_class(world,u.PGCharacterEnemy):
            ai=enemy.get_controller()
            if ai and isinstance(ai,u.PGRoleAIController): ai.set_combat_thinking_enabled(False)
        state=g.get_editor_property('state')
        # PIE initially copies the deliberately failed editor state. BeginPlay requests generation next tick.
        if phase=='ready' and state==u.PGDungeonState.FAILED and now-start<15: return
        definition=g.get_editor_property('definition')
        if phase=='missing':
            assert state==u.PGDungeonState.FAILED
            assert not u.GameplayStatics.get_all_actors_of_class(world,u.PGDungeonTreasure)
            definition.set_editor_property('decoration_graph',original_graph)
            g.generate(17); g.cancel_generation()
            assert g.get_editor_property('state')==u.PGDungeonState.CANCELLED
            assert not g.get_components_by_class(u.PCGComponent)
            phase='cancelled'; at=now
            return
        if phase=='cancelled' and now-at>.5:
            assert g.get_editor_property('state')==u.PGDungeonState.CANCELLED,'Late PCG completion escaped cancellation'
            assert not g.get_components_by_class(u.InstancedStaticMeshComponent)
            g.generate(101026); phase='recovered'; at=now
            return
        if phase=='cancelled': return
        assert state!=u.PGDungeonState.FAILED,g.get_editor_property('last_error')
        if state!=u.PGDungeonState.READY: return
        delay=3 if RENDER else .5
        if phase=='ready':
            # Ready already validated static routes before combat installed the locked boss seal.
            assert len(g.get_editor_property('selected_modules'))==len(g.get_editor_property('layout').rooms)
            assert len(g.get_components_by_class(u.PCGComponent))==1
            assert g.get_editor_property('decoration_point_count')>0
            report['points']=g.get_editor_property('decoration_point_count')
            report['modules']=[str(x) for x in g.get_editor_property('selected_modules')]
            report['layout']=[(r.cell.x,r.cell.y,str(r.role)) for r in g.get_editor_property('layout').rooms]
            caches=u.GameplayStatics.get_all_actors_of_class(world,u.PGDungeonTreasure)
            assert 1<=len(caches)<=3
            report['treasures']=len(caches)
            report['reward_rolls']=reward_signature(world)
            report['wall_collision']=[(c.get_name(),str(c.get_collision_enabled())) for c in g.get_components_by_class(u.InstancedStaticMeshComponent)]
            target=caches[0]
            command(world,'PGDungeonStep quartercamera')
            phase='entrance'; at=now
        elif phase=='entrance' and now-at>delay:
            capture(world,'01_entrance')
            player.set_actor_location(target.get_actor_location()+u.Vector(80,0,25),False,True)
            phase='treasure'; at=now
        elif phase=='treasure' and now-at>delay:
            capture(world,'02_treasure')
            command(world,'PGDungeonStep actioncamera')
            phase='treasure_action'; at=now
        elif phase=='treasure_action' and now-at>delay:
            capture(world,'02b_treasure_action')
            command(world,'PGDungeonStep treasure')
            command(world,'PGDungeonStep quartercamera')
            target=None; phase='claimed'; at=now
        elif phase=='claimed' and now-at>delay:
            assert len(u.GameplayStatics.get_all_actors_of_class(world,u.PGDungeonTreasure))==report['treasures']-1
            capture(world,'03_claimed')
            player.set_actor_location(g.get_actor_location()+u.Vector(1250,1250,100),False,True)
            if RENDER:
                # Static visual fixture using the production attack material; this does not test attack timing.
                center=g.get_actor_location()+u.Vector(1100,1100,2)
                material=u.load_asset('/Game/DataCenter/CombatCycle/M_EliteTelegraph'); assert material
                telegraph=u.GameplayStatics.spawn_decal_at_location(world,material,u.Vector(180,250,250),center,u.Rotator(pitch=-90,yaw=0,roll=0),0.)
                assert telegraph
                mid=telegraph.create_dynamic_material_instance(); assert mid
                mid.set_vector_parameter_value('Center',u.LinearColor(center.x,center.y,center.z,1))
                mid.set_vector_parameter_value('GradeColor',u.LinearColor(1,.18,.025,1))
                mid.set_scalar_parameter_value('Radius',230.)
                mid.set_scalar_parameter_value('Shape',0.)
                report['telegraph_visual_fixture']=True
            phase='quarter'; at=now
        elif phase=='quarter' and now-at>delay:
            report['quarter_faded']=g.get_editor_property('faded_instance_count')
            assert [(c.get_name(),str(c.get_collision_enabled())) for c in g.get_components_by_class(u.InstancedStaticMeshComponent)]==report['wall_collision']
            capture(world,'04_quarter_wall')
            command(world,'PGDungeonStep actioncamera')
            phase='action'; at=now
        elif phase=='action' and now-at>delay:
            report['action_faded']=g.get_editor_property('faded_instance_count')
            assert report['quarter_faded']>0 or report['action_faded']>0,'No environment occlusion observed'
            assert player.get_component_by_class(u.SpringArmComponent).get_editor_property('do_collision_test')
            capture(world,'05_action_wall')
            if telegraph: telegraph.destroy_component(telegraph); telegraph=None
            command(world,'PGDungeonStep quartercamera')
            # Return to a clear center and verify that fade is reversible.
            player.set_actor_location(g.get_actor_location()+u.Vector(0,0,100),False,True)
            phase='restored'; at=now
        elif phase=='restored' and now-at>delay:
            report['restored_faded']=g.get_editor_property('faded_instance_count')
            assert report['restored_faded']<max(report['quarter_faded'],report['action_faded'])
            player.set_actor_location(outside_boss(g),False,True)
            phase='gate_closed'; at=now
        elif phase=='gate_closed' and now-at>delay:
            assert g.get_editor_property('boss_gate_locked')
            capture(world,'06_boss_locked')
            u.GameplayStatics.set_global_time_dilation(world,8.)
            phase='progress'; at=now
        elif phase=='progress':
            manager=g.get_editor_property('combat_manager')
            stage=manager.get_current_stage_state()
            assert stage!=u.PGStageState.FAILED
            if stage==u.PGStageState.DUNGEON_TRAVERSAL:
                if manager.get_current_stage_id()==6:
                    u.GameplayStatics.set_global_time_dilation(world,1.)
                    player.set_actor_location(outside_boss(g),False,True)
                    phase='gate_open'; at=now
                else: player.set_actor_location(manager.get_dungeon_objective_location()+u.Vector(0,0,100),False,True)
            elif stage in (u.PGStageState.IN_PROGRESS,u.PGStageState.WAVE_INTERMISSION): command(world,'PGDungeonStep kill')
            elif stage==u.PGStageState.BUILD_PHASE:
                command(world,'PGDungeonStep reward'); manager.ready_for_next_stage()
        elif phase=='gate_open' and now-at>delay:
            assert not g.get_editor_property('boss_gate_locked')
            capture(world,'07_boss_open')
            player.set_actor_location(boss_center(g)+u.Vector(0,0,100),False,True)
            phase='boss_combat'; at=now
        elif phase=='boss_combat' and now-at>delay:
            assert g.get_editor_property('boss_gate_locked')
            capture(world,'08_boss_combat')
            command(world,'PGDungeonStep kill'); phase='boss_clear'; at=now
        elif phase=='boss_clear' and now-at>delay:
            assert not g.get_editor_property('boss_gate_locked')
            assert g.get_editor_property('combat_manager').get_current_stage_state()==u.PGStageState.FINISHED
            capture(world,'09_boss_clear')
            report['boss_gate_flow']=True
            # Decor count changes must not reroll layout/module selection or branch rewards.
            original_density=definition.get_editor_property('props_per_room')
            definition.set_editor_property('props_per_room',0)
            g.generate(101026); phase='density'; at=now
        elif phase=='density' and now-at>delay:
            assert [(r.cell.x,r.cell.y,str(r.role)) for r in g.get_editor_property('layout').rooms]==report['layout']
            assert [str(x) for x in g.get_editor_property('selected_modules')]==report['modules']
            assert g.get_editor_property('decoration_point_count')<report['points']
            assert reward_signature(world)==report['reward_rolls'],'Decoration density changed branch rewards'
            definition.set_editor_property('props_per_room',original_density)
            original_graph=definition.get_editor_property('decoration_graph')
            definition.set_editor_property('decoration_graph',None)
            g.generate(17); phase='missing'; at=now
        elif phase=='recovered' and now-at>delay:
            assert len(g.get_components_by_class(u.PCGComponent))==1
            assert g.get_editor_property('decoration_point_count')==report['points']
            assert len(u.GameplayStatics.get_all_actors_of_class(world,u.PGDungeonTreasure))==report['treasures']
            report['pcg_failure_cancel_recovery']=True
            if RENDER:
                assert u.PGEditorProbeTools.resize_play_viewport(world,1920,1080)
                phase='resolution'; at=now
            else: finish()
        elif phase=='resolution' and now-at>delay:
            capture(world,'10_resolution_1080')
            assert report['image_sizes']['10_resolution_1080']==(1920,1080)
            finish()
    except BaseException: finish(traceback.format_exc())

busy=False
def guarded(dt):
    global busy
    if busy: return
    busy=True
    try: tick(dt)
    finally: busy=False
handle=u.register_slate_post_tick_callback(guarded)

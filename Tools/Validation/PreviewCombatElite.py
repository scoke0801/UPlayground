"""Isolated rendered elite hit/miss/cancel probe against the shared radius."""
import unreal, os, time
root=unreal.Paths.project_dir()
exec(compile(open(os.path.join(root,'Tools/Validation/ConfigureCombatCycleAssets.py'),encoding='utf-8-sig').read(),'ConfigureCombatCycleAssets.py','exec'))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/StageDevMap')
elite_class=unreal.EditorAssetLibrary.load_blueprint_class('/Game/DataCenter/CombatCycle/BP_EliteWarden')
placed=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(elite_class,unreal.Vector(0,0,100))
placed.set_editor_property('tags',['PGEliteProbe'])
level.editor_request_begin_play()
probe_start=time.monotonic(); probe_elapsed=0.; probe_actor=None; probe_player=None; probe_phase=0; probe_next=0.; probe_origin=None
probe_tag=unreal.GameplayTag(); probe_tag.import_text('(TagName="Enemy.Ability.MeleeSkill")')

def pg_elite_tick(delta):
    global probe_elapsed,probe_actor,probe_player,probe_phase,probe_next,probe_origin
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world: return
    probe_elapsed+=delta
    if probe_phase==0 and probe_elapsed>3:
        probe_player=unreal.GameplayStatics.get_player_pawn(world,0)
        if not probe_player: return
        unreal.SystemLibrary.execute_console_command(world,'PGStress 0 0')
        probe_origin=probe_player.get_actor_location()
        probe_actor=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,elite_class) if a.actor_has_tag('PGEliteProbe'))
        probe_actor.set_actor_location_and_rotation(unreal.Vector(probe_origin.x-280,probe_origin.y,probe_origin.z),unreal.Rotator(),False,True)
        probe_phase=1; probe_next=probe_elapsed+1
    if probe_actor:
        for enemy in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
            if enemy!=probe_actor: enemy.destroy_actor()
        controller=probe_actor.get_controller()
        if controller:
            brain=controller.get_editor_property('brain_component')
            if brain: brain.stop_logic('Combat cycle elite probe')
    if probe_phase==1 and probe_elapsed>=probe_next:
        asc=probe_actor.get_editor_property('ability_system_component')
        result=asc.try_activate_ability_by_tag(probe_tag)
        unreal.log('PGEliteProbe inside activated='+str(result))
        probe_phase=2; probe_next=probe_elapsed+.45
    elif probe_phase==2 and probe_elapsed>=probe_next:
        unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI')
        probe_phase=3; probe_next=probe_elapsed+5.2
    elif probe_phase==3 and probe_elapsed>=probe_next:
        probe_player.set_actor_location(unreal.Vector(probe_origin.x+300,probe_origin.y,probe_origin.z),False,True)
        result=probe_actor.get_editor_property('ability_system_component').try_activate_ability_by_tag(probe_tag)
        unreal.log('PGEliteProbe outside activated='+str(result))
        probe_phase=4; probe_next=probe_elapsed+1.6
    elif probe_phase==4 and probe_elapsed>=probe_next:
        unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI')
        probe_phase=5; probe_next=probe_elapsed+.5
    elif probe_phase==5 and probe_elapsed>=probe_next:
        probe_actor.destroy_actor(); probe_actor=None
        unreal.log('PGEliteProbe COMPLETE')
        unreal.unregister_slate_post_tick_callback(probe_handle)
        level.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
    if time.monotonic()-probe_start>90:
        unreal.unregister_slate_post_tick_callback(probe_handle)
        level.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
probe_handle=unreal.register_slate_post_tick_callback(pg_elite_tick)


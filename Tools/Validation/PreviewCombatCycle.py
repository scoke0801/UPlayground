"""Render an isolated PIE preview; -PGPrepareOnly leaves existing disk assets untouched."""
import unreal, os, time
root=unreal.Paths.project_dir()
exec(compile(open(os.path.join(root,'Tools/Validation/ConfigureCombatCycleAssets.py'),encoding='utf-8-sig').read(),'ConfigureCombatCycleAssets.py','exec'))
exec(compile(open(os.path.join(root,'Tools/Validation/ValidateCombatCycleAssets.py'),encoding='utf-8-sig').read(),'ValidateCombatCycleAssets.py','exec'))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/StageDevMap')
level.editor_request_begin_play()
preview_started=time.monotonic()
preview_command_sent=False
preview_world_seen=0

def pg_preview_tick(delta):
    global preview_command_sent, preview_world_seen
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        preview_world_seen+=1
        if preview_world_seen>120 and not preview_command_sent:
            unreal.SystemLibrary.execute_console_command(world,'PGCombatCycleSmoke')
            preview_command_sent=True
        if preview_command_sent:
            stages=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGStageManager)
            if stages and stages[0].get_editor_property('current_stage_state')==unreal.PGStageState.FINISHED:
                unreal.log('PGCombatCycle PREVIEW FINISHED')
                unreal.unregister_slate_post_tick_callback(preview_handle)
                level.editor_request_end_play()
                unreal.SystemLibrary.quit_editor()
                return
    if time.monotonic()-preview_started>150:
        unreal.log_error('PGCombatCycle PREVIEW TIMEOUT')
        unreal.unregister_slate_post_tick_callback(preview_handle)
        level.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
preview_handle=unreal.register_slate_post_tick_callback(pg_preview_tick)

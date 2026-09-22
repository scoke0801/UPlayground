"""Run the saved MVP through real PIE, capture UI, and exit after all six stages.
Pass -PGTestProfile=unique-name. Does not modify authored assets.
"""
import unreal, time, os
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
level.editor_request_begin_play()
started=time.monotonic(); sent=False; ticks=0; captures=set()
def tick(dt):
    global sent,ticks
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    ticks+=1
    if world and ticks>120:
        stages=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGStageManager)
        if stages:
            stage=stages[0]; state=stage.get_current_stage_state()
            if not sent:
                unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI')
                stage.ready_for_next_stage()
                unreal.SystemLibrary.execute_console_command(world,'PGCombatCycleSmoke'); sent=True
            if state==unreal.PGStageState.BUILD_PHASE:
                key=stage.get_current_stage_id()
                if key not in captures:
                    captures.add(key); unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI')
            if state==unreal.PGStageState.FINISHED:
                unreal.log('PGRogue PREVIEW COMPLETE')
                unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI')
                unreal.unregister_slate_post_tick_callback(handle)
                level.editor_request_end_play(); unreal.SystemLibrary.quit_editor(); return
            if state==unreal.PGStageState.FAILED:
                unreal.log_error('PGRogue PREVIEW FAILED'); unreal.unregister_slate_post_tick_callback(handle)
                level.editor_request_end_play(); unreal.SystemLibrary.quit_editor(); return
    if time.monotonic()-started>480:
        unreal.log_error('PGRogue PREVIEW TIMEOUT'); unreal.unregister_slate_post_tick_callback(handle)
        level.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)

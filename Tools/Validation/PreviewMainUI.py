"""Run in a separate editor: -ExecCmds="py .../PreviewMainUI.py" -PGTestProfile=MainUI."""
import os
import time
import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/StageDevMap')
level.editor_request_begin_play()
started = time.monotonic()
captured = False

def tick(delta):
    global captured
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    elapsed = time.monotonic() - started
    if world and elapsed > 12 and not captured:
        captured = True
        unreal.SystemLibrary.execute_console_command(world, 'PGHUDCapture')
        unreal.log('PGMainUI: requested gameplay UI screenshot')
    if (captured and elapsed > 20) or elapsed > 60:
        if not captured:
            unreal.log_error('PGMainUI: preview timed out')
        unreal.unregister_slate_post_tick_callback(handle)
        level.editor_request_end_play()
        unreal.SystemLibrary.quit_editor()

handle = unreal.register_slate_post_tick_callback(tick)

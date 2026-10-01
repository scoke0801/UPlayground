"""Read-only PIE counterpart of -game PGCombatControlsProbe. Uses an isolated profile.

Run the full editor with -ExecutePythonScript=<this file> -PGTestProfile=<unique>
-PGCaptureProbe. Does not save maps or authored assets.
This probe supplies health assistance; it is not direct-input combat QA.
"""
import time
import unreal

assert "-PGTestProfile=" in unreal.SystemLibrary.get_command_line()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level("/Game/Maps/RogueArena")
level.editor_request_begin_play()
started = time.monotonic()
probe_started = None
stop_requested = None


def tick(_dt):
    global probe_started, stop_requested
    now = time.monotonic()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if probe_started is None and world and unreal.GameplayStatics.get_player_pawn(world, 0):
        unreal.SystemLibrary.execute_console_command(world, "t.MaxFPS 60")
        unreal.SystemLibrary.execute_console_command(world, "PGCombatControlsProbe")
        unreal.log("PGNavigation PIE probe started (Assisted)")
        probe_started = now
    elif probe_started is not None and stop_requested is None and now - probe_started > 19:
        level.editor_request_end_play()
        stop_requested = now
    elif stop_requested is not None and not world and now - stop_requested > 2:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log("PGNavigation PIE probe ended normally")
        unreal.SystemLibrary.quit_editor()
    elif now - started > 90:
        unreal.log_error("PGNavigation PIE probe failed to enter play")
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)

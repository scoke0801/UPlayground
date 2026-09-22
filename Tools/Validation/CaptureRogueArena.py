import unreal, time
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
level.editor_request_begin_play()
ready_at=None; captured=False
def tick(dt):
    global ready_at,captured
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world: return
    if ready_at is None: ready_at=time.monotonic()
    elapsed=time.monotonic()-ready_at
    if elapsed>6 and not captured:
        unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
        unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI')
        captured=True
    if elapsed>9:
        unreal.unregister_slate_post_tick_callback(handle)
        level.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)

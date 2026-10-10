"""Open a controllable, isolated Dark Knight encounter; leave PIE running."""
import time
import unreal

assert '-PGTestProfile=DarkKnightPlay' in unreal.SystemLibrary.get_command_line()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/RogueArena')
assert unreal.PGEditorProbeTools.begin_play_window()
started=time.monotonic()

def tick(dt):
    if time.monotonic()-started>90:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log_error('Dark Knight 플레이 시작 시간 초과')
        return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world or time.monotonic()-started<5:return
    if not unreal.GameplayStatics.get_player_pawn(world,0):return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.execute_console_command(world,'PGBossEncounter 15601')
    unreal.log('Dark Knight 전투 시작: 일반 프로필과 분리된 보조 플레이 기록입니다.')

handle=unreal.register_slate_post_tick_callback(tick)

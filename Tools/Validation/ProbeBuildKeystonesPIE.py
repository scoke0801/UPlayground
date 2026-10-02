"""Rendered fixed-target build comparison; synthetic hit events, never direct-input QA."""
import time
import unreal

assert '-PGTestProfile=' in unreal.SystemLibrary.get_command_line()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
# -immersive starts PIE before Python startup; the runner supplies the map explicitly.
if not level.is_in_play_in_editor():
    assert level.load_level('/Game/Maps/RogueArena')
    level.editor_request_begin_play()
started=time.monotonic()
stopping=None
elapsed=0.
at=2.
index=0
steps=[]

def command(value):
    unreal.SystemLibrary.execute_console_command(player,value)

def scenario(family,core):
    command('PGBuildScenario '+family+' '+str(int(core)))
    unreal.WidgetLibrary.set_focus_to_game_viewport()

def sample(label):
    command('PGBuildProbe '+label)
    unreal.log('PGBuildCapture '+label)
    command('Shot SHOWUI')

def hits(count):
    for _ in range(count): command('PGBuildProbe hit')

for family in ('Bleed','Shock','Frenzy'):
    steps += [(lambda f=family:scenario(f,False),.7), (lambda:command('PGBuildCards'),.7),
              (lambda f=family:sample(f+'_cards'),.5), (lambda f=family:scenario(f,False),.7)]
    for core in (False,True):
        label=family+('_after' if core else '_before')
        if core: steps.append((lambda f=family:scenario(f,True),.7))
        if family=='Bleed':
            steps += [(lambda:hits(2),.2)]
            if core: steps += [(lambda:sample('Bleed_stacks'),.2)]
            steps += [(lambda:command('PGBuildProbe burst_setup'),0.),(lambda:command('PGBuildProbe heavy'),.2)]
        elif family=='Shock':
            steps += [(lambda:command('PGBuildProbe heavy'),.95),(lambda:command('PGBuildProbe heavy'),.45)]
        else:
            steps += [(lambda:hits(4),.2),(lambda:command('PGBuildProbe roll'),.2)]
        steps.append((lambda l=label:sample(l),.6))

def finish():
    global stopping
    if stopping is None:
        level.editor_request_end_play()
        stopping=time.monotonic()

def tick(dt):
    global elapsed,at,index,player
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stopping is not None:
        if not world and time.monotonic()-stopping>3:
            player=None
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if time.monotonic()-started>120:
        unreal.log_error('PGBuildPresentation TIMEOUT');finish();return
    if not world:return
    elapsed+=dt
    player=unreal.GameplayStatics.get_player_pawn(world,0)
    if not player or elapsed<at:return
    try:
        if index>=len(steps):
            unreal.log('PGBuildPresentation COMPLETE');finish();return
        action,delay=steps[index]
        action();index+=1;at=elapsed+delay
    except Exception as error:
        unreal.log_error('PGBuildPresentation '+repr(error));finish()

handle=unreal.register_slate_post_tick_callback(tick)

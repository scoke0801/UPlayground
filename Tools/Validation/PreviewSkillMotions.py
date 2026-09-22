"""Read-only PIE animation contact-frame capture. Never saves the map or assets."""
import unreal, json, os, time
root=unreal.Paths.project_dir()
audit=json.load(open(os.path.join(root,'Saved/SkillMotionAudit.json'),encoding='utf-8'))
shots=[]
for row in audit['skills']:
 if row['SkillID'] not in [100,101,102,110,111,112,113,114,10000]:continue
 for path in row.get('dependencies',[]):
  asset=unreal.load_asset(path) if path.startswith('/Game/') else None
  if isinstance(asset,unreal.AnimSequence):
   for fraction in [.2,.4,.6]: shots.append((row['SkillID'],asset,asset.get_play_length()*fraction))
   break
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.load_level('/Game/Maps/StageDevMap')
level.editor_request_begin_play()
started=time.monotonic(); last=0; index=0; player=None; armed=False
manifest=[]
def tick(dt):
 global player,last,index,armed
 try:
  now=time.monotonic()
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if now-started>100: raise RuntimeError('capture timeout')
  if not world or now-started<6:return
  if player is None:
   player=unreal.GameplayStatics.get_player_character(world,0)
   if not player:return
   unreal.WidgetLayoutLibrary.remove_all_widgets(world)
   player.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
   player.set_actor_tick_enabled(False)
   player.set_actor_rotation(unreal.Rotator(0,0,0),False)
   for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PGCharacterEnemy):
    actor.set_actor_hidden_in_game(True); actor.set_actor_enable_collision(False); actor.set_actor_tick_enabled(False)
   boom=player.get_component_by_class(unreal.SpringArmComponent)
   boom.set_editor_property('target_arm_length',430.)
   boom.set_editor_property('enable_camera_lag',False)
   boom.set_world_rotation(unreal.Rotator(-15,-55,0),False,False)
   unreal.GameplayStatics.set_global_time_dilation(world,0.0001)
   player.mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
  if now-last<.6:return
  if index>=len(shots):
   with open(os.path.join(root,'Saved/SkillMotionFrames.json'),'w') as f:json.dump(manifest,f,indent=2)
   unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor();return
  skill,anim,pos=shots[index]
  if not armed:
   player.mesh.set_animation(anim)
   player.mesh.set_position(pos,False)
   armed=True
  else:
   filename='SkillMotion_%s_%02d'%(skill,index)
   unreal.SystemLibrary.execute_console_command(world,'Shot filename='+filename+' -nosuffix')
   manifest.append({'skill':skill,'animation':anim.get_path_name(),'time':pos,'file':filename})
   index+=1;armed=False
  last=now
 except Exception as e:
  unreal.log_error('PG motion capture: '+str(e));unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
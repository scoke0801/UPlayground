"""Save reusable toon character Blueprints and render the five-character gallery."""
import json
import os
import sys
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=Path(os.environ.get('PG_CHARACTER_BATCH_OUTPUT',ROOT/'Saved/ToonCharacters'))
DATA=json.loads(Path(os.environ.get('PG_CHARACTER_BATCH_MANIFEST',ROOT/'Tools/Art/ToonCharacters/manifest.json')).read_text(encoding='utf-8'))
AUDIT=json.loads((OUT/'configure.json').read_text(encoding='utf-8'))
assert AUDIT['status']=='PASS'
DEST=DATA['destination']
MAP=DEST+'/Maps/'+DATA.get('gallery_name','L_PG_ToonCharacters')
REPORT=dict(status='RUNNING',map=MAP,blueprints={},images=[])
EAL=unreal.EditorAssetLibrary
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVEL=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def save(asset):
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()


def mesh_setup(comp,mesh):
    comp.set_skeletal_mesh_asset(mesh)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_render_custom_depth(True)
    comp.set_custom_depth_stencil_value(73)
    comp.set_editor_property('cast_shadow',True)


def blueprint(row):
    name=row['name']
    path=DEST+'/'+name+'/BP_PG_'+name+'_Toon'
    bp=unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if not bp:
        factory=unreal.BlueprintFactory()
        factory.set_editor_property('parent_class',unreal.PGToonPreviewActor)
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_PG_'+name+'_Toon',DEST+'/'+name,unreal.Blueprint,factory)
    cdo=unreal.get_default_object(bp.generated_class())
    mesh=unreal.load_asset(row['mesh'])
    mesh_setup(cdo.skeletal_mesh_component,mesh)
    # Use a registered component to inspect the imported skeleton and calibrate axes.
    probe=ACTORS.spawn_actor_from_class(unreal.PGToonPreviewActor,unreal.Vector())
    mesh_setup(probe.skeletal_mesh_component,mesh)
    comp=probe.skeletal_mesh_component
    bone=next((comp.get_bone_name(i) for i in range(comp.get_num_bones())
               if str(comp.get_bone_name(i)).lower() in ('head','head_x','j_bip_c_head')),None)
    assert bone,(name,[str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())])
    head=comp.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT)
    presentation=cdo.toon_presentation
    presentation.set_editor_property('head_bone',bone)
    presentation.set_editor_property('head_forward_axis',unreal.MathLibrary.inverse_transform_direction(head,unreal.Vector(0,1,0)))
    presentation.set_editor_property('head_right_axis',unreal.MathLibrary.inverse_transform_direction(head,unreal.Vector(1,0,0)))
    ACTORS.destroy_actor(probe)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    REPORT['blueprints'][name]=dict(asset=path,mesh=row['mesh'],head_bone=str(bone))
    return bp


def build():
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    mode=unreal.load_asset('/Game/Art/ToonTest/Advanced/BP_PGToonLabGameMode')
    world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
    unreal.SystemLibrary.execute_console_command(world,'r.CustomDepth 3')
    pp=ACTORS.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
    pp.set_editor_property('unbound',True)
    settings=unreal.PostProcessSettings()
    for k,v in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True,auto_exposure_bias=0.,override_bloom_intensity=True,bloom_intensity=0.,
        override_motion_blur_amount=True,motion_blur_amount=0.).items(): settings.set_editor_property(k,v)
    pp.set_editor_property('settings',settings)
    pp.add_or_update_blendable(unreal.load_asset('/Game/Art/ToonTest/Advanced/Materials/M_PGToonScreenOutline'),1.)
    floor=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-6))
    floor.set_actor_label('PG Toon Character Gallery Floor')
    floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/Art/UtilityModels/SM_PG_DevBlock_Centered'))
    floor.static_mesh_component.set_material(0,unreal.load_asset('/Game/Art/ToonTest/Advanced/Materials/M_PGToonLabFloor'))
    floor.set_actor_scale3d(unreal.Vector(24,18,.1))
    key=ACTORS.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,300),unreal.Rotator(pitch=-55,yaw=-90))
    key.set_actor_label('PG Toon Key')
    key.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    key.light_component.set_intensity(3.2)
    fill=ACTORS.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,300),unreal.Rotator(pitch=-35,yaw=70))
    fill.light_component.set_intensity(.45)
    fill.light_component.set_cast_shadows(False)
    bases={}
    for i,row in enumerate(AUDIT['characters']):
        bp=blueprint(row)
        actor=ACTORS.spawn_actor_from_class(bp.generated_class(),unreal.Vector((i-(len(AUDIT['characters'])-1)/2)*180,0,0))
        actor.set_actor_label('PG Toon '+row['name'])
        assert actor.skeletal_mesh_component.get_skeletal_mesh_asset(),row['name']
        actor.toon_presentation.set_editor_property('key_light',key)
        bases[row['name']]=actor
    camera=ACTORS.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,1100,270))
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,90)),False)
    camera.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
    camera.camera_component.set_editor_property('field_of_view',55.)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
    for actor in bases.values(): actor.toon_presentation.initialize(actor.skeletal_mesh_component)
    for cmd in ('DisableAllScreenMessages','r.ScreenPercentage 100','r.Streaming.FullyLoadUsedTextures 1','viewmode lit'):
        unreal.SystemLibrary.execute_console_command(world,cmd)
    LEVEL.editor_set_game_view(True)
    return world,bases,camera


def finish(error=None):
    REPORT['status']='FAIL' if error else 'CAPTURED'
    if error: REPORT['error']=error
    (OUT/'preview.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    if 'handle' in globals(): unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global index,prepared,pending,last
    try:
        now=time.monotonic()
        if now-started>480: return finish('Render timeout')
        if now-started<35 or now-last<3: return
        if pending:
            if not pending.exists() or pending.stat().st_size<10000: return
            REPORT['images'].append(str(pending))
            index+=1
            prepared,pending,last=False,None,now
            if index==len(shots): return finish()
            return
        label,mode=shots[index]
        if prepared:
            pending=OUT/(label+'_'+mode+'.png')
            if pending.exists(): pending.unlink()
            unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+pending.as_posix()+'"')
            last=now
            return
        for name,actor in bases.items(): actor.set_is_temporarily_hidden_in_editor(label!='All' and name!=label)
        if label=='All':
            target,loc,fov=unreal.Vector(0,0,85),unreal.Vector(0,1100,270),55.
        else:
            actor=bases[label]
            target=actor.get_actor_location()+unreal.Vector(0,0,90)
            loc,fov=target+unreal.Vector(70,380,45),48.
            if mode=='Face':
                target=actor.skeletal_mesh_component.get_socket_location(actor.toon_presentation.head_bone)+unreal.Vector(0,0,5)
                loc,fov=target+unreal.Vector(9,105,5),35.
            elif mode=='Quarter': loc=target+unreal.Vector(240,350,310)
        camera.set_actor_location(loc,False,False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(loc,target),False)
        camera.camera_component.set_editor_property('field_of_view',fov)
        LEVEL.pilot_level_actor(camera,'None')
        prepared,last=True,now
    except Exception: finish(traceback.format_exc())


try:
    world,bases,camera=build()
    shots=[('All','Gallery')]+[(name,view) for name in bases for view in ('Full','Face','Quarter')]
    index,prepared,pending=0,False,None
    started=last=time.monotonic()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    handle=unreal.register_slate_post_tick_callback(tick)
except Exception: finish(traceback.format_exc())

"""Save and render paginated Bokusei Katana galleries; verify evaluated poses and PIE clocks."""
import json
import math
import shutil
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal
ROOT = Path(unreal.Paths.project_dir()).resolve()
AUDIT = json.loads((ROOT/'Saved/BokuseiMotionLibrary/configure.json').read_text(encoding='utf-8'))
assert AUDIT['status']=='PASS'
OUT = ROOT/'Saved/BokuseiMotionLibrary/Preview'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
LATEST = ROOT/'Saved/BokuseiMotionLibrary/preview.json'
REPORT = dict(status='RUNNING',run=str(OUT),pages=[],images=[])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
mesh = unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
outline_slots = [dict(asset='/Game/Art/ToonTest/Bokusei/Materials/MI_PGToonOutline_Bokusei_'+str(s.material_slot_name)) for s in mesh.get_editor_property('materials')]

def build_page(page):
    global world, components, outlines, locations, camera_location, rotation
    global page_report
    page_report = dict(name=page['name'],map=page['map'])
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    assert world
    world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
    volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    volume.set_actor_label('PG Toon Fixed Exposure')
    volume.set_editor_property('unbound', True)
    settings = unreal.PostProcessSettings()
    for key, value in {
        'override_auto_exposure_method': True,
        'auto_exposure_method': unreal.AutoExposureMethod.AEM_MANUAL,
        'override_auto_exposure_apply_physical_camera_exposure': True,
        'auto_exposure_apply_physical_camera_exposure': False,
        'override_auto_exposure_bias': True,
        'auto_exposure_bias': 0.0,
        'override_bloom_intensity': True,
        'bloom_intensity': 0.0,
    }.items():
        settings.set_editor_property(key, value)
    volume.set_editor_property('settings', settings)
    components = []
    outlines = []
    locations = []
    TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
    
    
    def stage_material():
        path = '/Game/Art/ToonTest/Materials/M_PGToonPreviewFloor'
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return unreal.load_asset(path)
        mat = TOOLS.create_asset('M_PGToonPreviewFloor', '/Game/Art/ToonTest/Materials', unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        color = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, 0, 0)
        color.set_editor_property('constant', unreal.LinearColor(.045, .06, .095, 1))
        unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
        return mat
    
    
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -8))
    floor.set_actor_label('PG Toon Preview Floor')
    floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    floor.static_mesh_component.set_material(0, stage_material())
    floor.set_actor_scale3d(unreal.Vector(13, 11, .1))
    label_light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 250), unreal.Rotator(pitch=-55, yaw=-45, roll=0))
    label_light.set_actor_label('PG Toon Label Light (unlit characters unaffected)')
    label_light.light_component.set_editor_property('intensity', 3.0)
    label_light.light_component.set_editor_property('cast_shadows', False)
    
    for index, entry in enumerate(page['clips']):
        right = (index % 3 - 1) * 285
        away = (index // 3 - .5) * 260
        pos = unreal.Vector((right+away)/math.sqrt(2), (right-away)/math.sqrt(2), 0)
        locations.append(pos)
        base = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, pos)
        base.set_actor_label('PG Toon ' + entry['label'])
        comp = base.skeletal_mesh_component
        comp.set_skeletal_mesh_asset(mesh)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        anim = unreal.load_asset(entry['asset'])
        play_data = unreal.SingleAnimationPlayData()
        play_data.set_editor_property('anim_to_play', anim)
        play_data.set_editor_property('saved_looping', True)
        play_data.set_editor_property('saved_playing', True)
        play_data.set_editor_property('saved_position', entry['duration'] * entry['probe_fraction'])
        comp.set_editor_property('animation_data', play_data)
        comp.set_animation(anim)
        comp.set_position(entry['duration'] * entry['probe_fraction'], False)
        comp.set_update_animation_in_editor(True)
        comp.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        comp.set_editor_property('cast_shadow', False)
        comp.set_play_rate(0)
        components.append(comp)
        hull = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, pos)
        hull.set_actor_label('PG Toon ' + entry['label'] + ' Outline')
        outlines.append(hull)
        hull_comp = hull.skeletal_mesh_component
        hull_comp.set_skeletal_mesh_asset(mesh)
        hull_comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        hull_comp.set_editor_property('cast_shadow', False)
        hull_comp.set_leader_pose_component(comp)
        for slot in range(hull_comp.get_num_materials()):
            hull_comp.set_material(slot, unreal.load_asset(outline_slots[slot]['asset']))
        label = actors.spawn_actor_from_class(unreal.TextRenderActor, pos + unreal.Vector(-75, 75, 5), unreal.Rotator(pitch=55, yaw=135, roll=0))
        label.set_actor_label('PG Toon Label ' + entry['label'])
        label.text_render.set_text(entry['label'])
        label.text_render.set_world_size(17)
        label.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    
    target = unreal.Vector(0, 0, 55)
    rotation = unreal.Rotator(pitch=-55, yaw=-45, roll=0)
    camera_location = target - unreal.MathLibrary.get_forward_vector(rotation) * 1200
    camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_location, rotation)
    camera.set_actor_label('PG Toon Quarter View 1200cm')
    camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
    camera.camera_component.set_editor_property('field_of_view', 60)
    level.set_level_viewport_camera_info(camera_location, rotation, 'None')
    level.editor_set_game_view(True)
    level.set_level_viewport_fov(60, 'None')
    page_report['camera_rotation'] = {'pitch': rotation.pitch, 'yaw': rotation.yaw, 'roll': rotation.roll}
    page_report['camera_distance_cm'] = 1200
    # Protect an existing generated test map on reruns.
    map_file = ROOT / ('Content/'+page['map'].removeprefix('/Game/')+'.umap')
    if map_file.is_file():
        shutil.copy2(map_file, OUT / (page['name']+'_previous.umap'))
    # Save looping playback settings; snapshots below only affect this editor session.
    for comp in components:
        comp.set_play_rate(1)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, page['map'])
    for comp in components:
        comp.set_play_rate(0)
    

    for command in ['DisableAllScreenMessages','r.ScreenPercentage 100','r.Streaming.FullyLoadUsedTextures 1','viewmode lit']:
        unreal.SystemLibrary.execute_console_command(world,command)

started = time.monotonic()
last = started
page_index = 0
state = 'BUILD'
pending = None
pie_first = None
finished = False
busy = False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def finish(error=None):
    global finished
    if finished:
        return
    finished=True
    REPORT['status']='FAIL' if error else 'CAPTURED'
    REPORT['visual_review_required']=True
    REPORT['pie_playback']=dict(status='FAIL' if error else 'PASS',pages=len(REPORT['pages']))
    if error:
        REPORT['error']=error
        unreal.log_error(error)
    payload=json.dumps(REPORT,ensure_ascii=False,indent=2)
    (OUT/'preview.json').write_text(payload,encoding='utf-8')
    LATEST.write_text(payload,encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def tick(_dt):
    global last,state,page_index,pending,pie_first,busy
    if busy or finished:
        return
    busy=True
    try:
        now=time.monotonic()
        if now-started>850:
            raise RuntimeError('Gallery render timeout')
        page=AUDIT['pages'][page_index]
        if state=='BUILD':
            build_page(page)
            state='WARMUP'
            last=now
            return
        if state=='WARMUP':
            if now-last < (20 if page_index==0 else 4):
                return
            errors=[]
            for comp,entry in zip(components,page['clips']):
                anim=unreal.load_asset(entry['asset'])
                stamp=entry['duration']*entry['probe_fraction']
                comp.set_position(stamp,False)
                comp.override_animation_data(anim,True,False,stamp,0)
                options=unreal.AnimPoseEvaluationOptions()
                # Enabled root motion locks the root in single-node preview.
                options.set_editor_property('incorporate_root_motion_into_pose',False)
                options.set_editor_property('extract_root_motion',entry['group']=='Root_Motion')
                pose=anim.get_anim_pose_at_time(stamp,options)
                origin=comp.get_outer().get_actor_location()
                for bone in ['Head','Hand_L','Hand_R','Foot_L','Foot_R']:
                    expected=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
                    actual=comp.get_socket_location(bone)-origin
                    errors.append(math.dist([expected.x,expected.y,expected.z],[actual.x,actual.y,actual.z]))
            assert max(errors)<.1,('Frozen pose mismatch',page['name'],max(errors))
            page_report['capture_max_error_cm']=max(errors)
            pending=OUT/(page['name']+'.png')
            unreal.SystemLibrary.execute_console_command(world,f'HighResShot 1280x720 filename="{pending.as_posix()}"')
            state='CAPTURE'
            last=now
            return
        if state=='CAPTURE':
            if now-last<3 or not pending.is_file() or pending.stat().st_size<10000:
                return
            REPORT['images'].append(str(pending))
            for comp,entry in zip(components,page['clips']):
                comp.override_animation_data(unreal.load_asset(entry['asset']),True,True,0,1)
            level.editor_request_begin_play()
            state='PIE'
            pie_first=None
            last=now
            return
        if state=='PIE':
            game_world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not game_world or now-last<1:
                return
            instances=unreal.GameplayStatics.get_all_actors_of_class(game_world,unreal.SkeletalMeshActor)
            bases=[a.skeletal_mesh_component for a in instances if not a.get_actor_label().endswith(' Outline')]
            hulls=[a.skeletal_mesh_component for a in instances if a.get_actor_label().endswith(' Outline')]
            assert len(bases)==len(page['clips'])==len(hulls)
            assert all(c.is_playing() and abs(c.get_play_rate()-1)<.001 for c in bases)
            assert all(c.get_editor_property('leader_pose_component').get_world()==game_world for c in hulls)
            clocks={c.get_outer().get_actor_label():c.get_position() for c in bases}
            if pie_first is None:
                pie_first=clocks
                last=now
                return
            assert all(abs(clocks[k]-pie_first[k])>.0001 for k in clocks),'PIE clock did not advance'
            page_report['pie_playback']=dict(status='PASS',actors=len(bases),first=pie_first,second=clocks)
            REPORT['pages'].append(page_report)
            level.editor_request_end_play()
            state='END_PIE'
            last=now
            return
        if state=='END_PIE':
            if now-last<2 or unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
                return
            page_index+=1
            if page_index==len(AUDIT['pages']):
                finish()
                return
            state='BUILD'
    except Exception:
        finish(traceback.format_exc())
    finally:
        busy=False
handle=unreal.register_slate_post_tick_callback(tick)

"""Hand-authored pre-PCG sanctuary art pass. Only saves the existing test map.

Original marketplace assets are read-only. The existing floor, four battle-island
colliders, PlayerStart, GameMode and navigation bounds are retained. All additions
are map-owned and tagged for repeatable replacement; every run backs up the map.
"""
from datetime import datetime
import json
import math
from pathlib import Path
import random
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT/'Saved/LevelDressing'
MAP = '/Game/Maps/L_PG_ForestRuins'
TAG = 'PG_SanctuaryArt'
DATA = json.loads((OUT/'inventory.json').read_text(encoding='utf-8'))['meshes']
A = u.get_editor_subsystem(u.EditorActorSubsystem)
L = u.get_editor_subsystem(u.LevelEditorSubsystem)
R = random.Random(101026)
report = dict(map=MAP, placements=[], removed_foliage={}, build_tests_run=False)
cache = {}

def sanctuary_ground():
    """World-space stone/soil blend: a worn court and broad readable approach."""
    path='/Game/Environment/ForestRuins/Sanctuary'
    name='M_PG_SanctuaryGround'
    mat=u.load_asset(path+'/'+name)
    if not mat:mat=u.AssetToolsHelpers.get_asset_tools().create_asset(name,path,u.Material,u.MaterialFactoryNew())
    lib=u.MaterialEditingLibrary
    lib.delete_all_material_expressions(mat)
    def node(cls):return lib.create_material_expression(mat,cls)
    def link(a,pin,b,slot):assert lib.connect_material_expressions(a,pin,b,slot)
    def out(a,pin,prop):assert lib.connect_material_property(a,pin,prop)
    def const(value):
        n=node(u.MaterialExpressionConstant);n.set_editor_property('r',value);return n
    pos=node(u.MaterialExpressionWorldPosition)
    def custom(code,inputs,kind=u.CustomMaterialOutputType.CMOT_FLOAT1):
        n=node(u.MaterialExpressionCustom);pins=[]
        for key,(source,pin) in inputs.items():
            p=u.CustomInput();p.set_editor_property('input_name',key);pins.append(p)
        n.set_editor_property('inputs',pins);n.set_editor_property('output_type',kind);n.set_editor_property('code',code)
        for key,(source,pin) in inputs.items():link(source,pin,n,key)
        return n
    uv=custom('return P.xy/420.;',{'P':(pos,'')},u.CustomMaterialOutputType.CMOT_FLOAT2)
    def tex(path,normal=False):
        n=node(u.MaterialExpressionTextureSample);t=u.load_asset(path);assert t,path
        n.set_editor_property('texture',t)
        if normal:n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        link(uv,'',n,'UVs');return n
    base='/Game/ExternalAssets/LevelDesign/Dungeon_Pack/Assets/Textures/'
    stone=tex(base+'T_DungeonFloor_D')
    normal=tex(base+'T_DungeonFloor_N',True)
    wear=tex('/Game/ExternalAssets/LevelDesign/RuinedCrypt/Environment/TileGround_01/t_TileGround_01_01_bc')
    soil=tex('/Game/Environment/ForestRuins/Textures/HD_T_EastLands_ForestGround_01_C')
    moss=tex('/Game/Environment/ForestRuins/Textures/HD_T_EastLands_Moss_CR')
    mask=custom('''float n=sin(P.x*.012+sin(P.y*.006)*2.)*sin(P.y*.009)*70.+sin(P.x*.032+P.y*.019)*18.;
        float court=1.-smoothstep(820.,1170.,length(P.xy)+n);
        float approach=(1.-smoothstep(230.,410.,abs(P.x-(350.+sin(P.y*.0014)*120.))+n*.45))*(1.-smoothstep(1720.,1950.,abs(P.y)));
        float cross=(1.-smoothstep(145.,260.,abs(P.y-240.-P.x*.10)+n*.35))*(1.-smoothstep(1840.,2140.,abs(P.x)));
        float chapel=(1.-smoothstep(500.,650.,abs(P.x-700.)))*(1.-smoothstep(530.,670.,abs(P.y+2700.)));
        return saturate(max(chapel,max(court,max(approach,cross))));''',{'P':(pos,'')})
    color=custom('''float n=sin(P.x*.006+sin(P.y*.004))*sin(P.y*.007);
        float3 dirt=lerp(Soil*.78,Moss*.8,saturate(.40+n*.26));
        float3 stone=lerp(Stone*float3(.72,.70,.64),Wear*.48,.27);
        stone=lerp(stone,Moss*.70,saturate((1.-M)*.32+(n-.35)*.20));
        float r=length(P.xy);
        float ring=(1.-smoothstep(7.,11.,abs(r-670.)))*.32;
        ring+= (1.-smoothstep(3.,6.,abs(r-720.)))*.22;
        ring*=smoothstep(-.85,-.3,sin(atan2(P.y,P.x)*7.+.7));
        stone=lerp(stone,float3(.22,.17,.105),ring);
        return lerp(dirt,stone,M);''',{'P':(pos,''),'Soil':(soil,'RGB'),'Moss':(moss,'RGB'),
            'Stone':(stone,'RGB'),'Wear':(wear,'RGB'),'M':(mask,'')},u.CustomMaterialOutputType.CMOT_FLOAT3)
    out(color,'',u.MaterialProperty.MP_BASE_COLOR)
    n=custom('return normalize(lerp(float3(0,0,1),N,M*.38));',{'N':(normal,'RGB'),'M':(mask,'')},u.CustomMaterialOutputType.CMOT_FLOAT3)
    out(n,'',u.MaterialProperty.MP_NORMAL)
    out(const(.9),'',u.MaterialProperty.MP_ROUGHNESS)
    out(const(.12),'',u.MaterialProperty.MP_SPECULAR)
    lib.recompile_material(mat)
    assert u.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

def owned(actor, label, group):
    actor.set_actor_label(label)
    actor.set_folder_path('PG 폐성소/'+group)
    actor.set_editor_property('tags',[u.Name(TAG)])
    return actor

def place(name, x, y, width=None, height=None, yaw=0, z=0, scale=None, group='석조 유적', label=None):
    row = DATA[name]
    size = [b-a for a,b in zip(row['min'],row['max'])]
    if scale is None:
        scale = width/max(size[:2]) if width else height/size[2] if height else 1.
    if isinstance(scale,(int,float)): scale=(scale,)*3
    cx,cy = [(row['min'][i]+row['max'][i])*.5*scale[i] for i in range(2)]
    angle=math.radians(yaw)
    location=u.Vector(x-cx*math.cos(angle)+cy*math.sin(angle),
        y-cx*math.sin(angle)-cy*math.cos(angle), z-row['min'][2]*scale[2])
    mesh=cache.setdefault(name,u.load_asset(row['path']))
    assert isinstance(mesh,u.StaticMesh),name
    actor=owned(A.spawn_actor_from_class(u.StaticMeshActor,location,u.Rotator(yaw=yaw)),label or name,group)
    comp=actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_collision_profile_name('NoCollision')
    actor.set_actor_scale3d(u.Vector(*scale))
    if group in ['바닥 포장','잔해']: comp.set_cast_shadow(False)
    report['placements'].append(dict(label=label or name,mesh=row['path'],center=[x,y,z],scale=list(scale),yaw=yaw,group=group))
    return actor

def light(x,y,z,radius=430,intensity=160):
    actor=owned(A.spawn_actor_from_class(u.PointLight,u.Vector(x,y,z)),'봉헌의 불빛','조명')
    comp=actor.light_component
    comp.set_mobility(u.ComponentMobility.MOVABLE)
    comp.set_intensity(intensity)
    comp.set_light_color(u.LinearColor(1.,.38,.105))
    comp.set_editor_property('attenuation_radius',radius)
    comp.set_cast_shadows(False)

def paving(x,y,width=370,yaw=0,variant=0):
    name='sm_GroundStones_01_0'+str(1+variant%2)
    row=DATA[name];size=[b-a for a,b in zip(row['min'],row['max'])]
    s=width/max(size[:2])
    return place(name,x,y,yaw=yaw,z=.5,scale=(s,s,2.4/size[2]),group='바닥 포장',label='흙에 잠긴 참배길')

def rubble(x,y,count=8):
    for i in range(count):
        a=R.random()*math.tau;r=R.uniform(40,210)
        place('sm_Debris_0'+str(1+i%4)+'_01',x+math.cos(a)*r,y+math.sin(a)*r,
            width=R.uniform(55,140),yaw=R.randrange(360),z=-1,group='잔해',label='무너진 석재 조각')

def candles(x,y,z=0):
    for i,(dx,dy,h) in enumerate([(-20,0,28),(12,5,40),(30,-12,19)]):
        place('sm_Candle_01_01',x+dx,y+dy,height=h,z=z,group='봉헌 소품',label='봉헌 초')
    light(x,y,z+45,220,30)

assert L.load_level(MAP)
backup=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True)
shutil.copy2(ROOT/'Content/Maps/L_PG_ForestRuins.umap',backup/'L_PG_ForestRuins.umap')
report['backup']=str(backup)
actors=list(A.get_all_level_actors())
def gameplay_snapshot():
    snapshot={}
    for a in A.get_all_level_actors():
        if isinstance(a,(u.PlayerStart,u.NavMeshBoundsVolume,u.RecastNavMesh)) or (
                isinstance(a,u.StaticMeshActor) and str(a.get_folder_path()) in ['지형','전투 지형']):
            p=a.get_actor_location();s=a.get_actor_scale3d();r=a.get_actor_rotation()
            snapshot[a.get_actor_label()]=dict(cls=a.get_class().get_path_name(),
                location=[p.x,p.y,p.z],scale=[s.x,s.y,s.z],rotation=[r.pitch,r.yaw,r.roll])
    return snapshot
protected_before=gameplay_snapshot()
for actor in actors:
    if TAG in [str(t) for t in actor.tags]: A.destroy_actor(actor)
    elif isinstance(actor,u.InstancedFoliageActor):
        # Remove the old focal sculptures and sparse paving from the map only.
        for comp in actor.get_components_by_class(u.InstancedStaticMeshComponent):
            mesh=comp.static_mesh
            if mesh and any(s in mesh.get_name() for s in ['StoneArch','Fox_Statue','SpiritStatue','FlatStone','MedBoulder']):
                report['removed_foliage'][mesh.get_name()]=comp.get_instance_count()
                comp.clear_instances()
            elif mesh:
                removed=0
                for i in reversed(range(comp.get_instance_count())):
                    t=comp.get_instance_transform(i,True)
                    p=t.translation
                    if -450<p.x<1870 and -3540<p.y<-2020:
                        comp.remove_instance(i);removed+=1
                if removed:report['removed_foliage'][mesh.get_name()]=removed

# A broad, worn ceremonial floor leaves combat silhouettes readable.
# Marketplace geometry supplies the stone detail; it sits flush above the existing collider.
ground=sanctuary_ground()
for actor in A.get_all_level_actors():
    if actor.get_actor_label()=='연속 지면':actor.static_mesh_component.set_material(0,ground)
for i in range(8):
    y=-1760+i*350
    x=280+math.sin(i*.63)*80
    if abs(y)>1150:
        paving(x+R.uniform(-160,160),y,220,R.uniform(-20,20),i)
for sign in [-1,1]:
    for i in range(3):paving(sign*(1190+i*245),250+sign*i*70,230,R.uniform(-15,15),i)

# Roofless chapel in the background: twin bays, broken asymmetrical side walls.
# All substantial additions sit outside the original playable boundary.
for j,y in enumerate([-2330,-2920]):
    for x in [145,440,960,1255]:
        place('sm_ChapelColumnBase_01_01',x,y,width=205,z=-5,label='예배당 주춧돌')
        place('sm_ChapelColumn_01_01',x,y,height=790,z=38,label='예배당 기둥')
        place('sm_ChapelColumnTop_01_01',x,y,width=208,z=828,label='예배당 주두')
    place('sm_ChapelArch_01_01',700,y,width=1230,z=822,label='무너진 예배당 첨두 아치')
for x in [-165,1565]:
    place('sm_ChapelWall_01_03',x,-2660,height=810,yaw=90,label='예배당 창벽')
    rubble(x,-2320,12)
place('sm_ChapelWall_03_01',-520,-2350,width=800,yaw=8,label='무너진 예배당 왼쪽 날개')
place('sm_ChapelWall_03_02',1780,-2380,width=690,yaw=-12,label='무너진 예배당 오른쪽 날개')
place('sm_CryptEntrance_01_01',700,-3250,height=660,yaw=180,label='봉인된 지하묘지')
place('sm_Stairs_01_01',700,-2090,width=1000,z=-15,label='성소 계단')
for x in [10,1390]:
    place('sm_ChapelColumnPedestal_01_01',x,-2120,height=155,label='수호자 석좌')
    place('sm_GraveSculpture_01_01',x,-2120,height=335,z=155,yaw=180,label='성소의 수호자')
    candles(x,-2010)
light(700,-2780,200,900,150)

# Boundary rhythm: low ruined bays, with taller pieces only on the far side.
for x in [-1980,-1430,-880,1950]:
    place('sm_ChapelWall_03_02',x,-2090,width=480,height=None,z=-25,yaw=R.uniform(-5,5),label='담장 뒤 무너진 회랑')
    rubble(x,-2060,5)
for y in [-1350,-650,100,850,1500]:
    place('sm_ChapelColumn_01_04',2510,y,height=R.uniform(230,430),label='동쪽 회랑의 기둥 잔재')
    place('sm_ChapelColumnBase_01_01',2510,y,width=145,z=-5,label='회랑 주춧돌')
    rubble(2450,y,4)

# Quiet grave garden and abandoned pilgrim supplies, beyond the combat boundary.
for i in range(9):
    x=-2560-(i%3)*220;y=-750+(i//3)*390
    place('sm_Grave_01_01' if i%3 else 'sm_GraveSculpture_02_01',x,y,
          height=R.uniform(155,245),yaw=85+R.uniform(-9,9),label='숲에 잠긴 묘비',group='서쪽 묘역')
    paving(x+60,y,210,90,i)
for y in [-1050,700]:
    place('sm_Fence_02_02',-2730,y,width=600,yaw=90,z=-20,label='부서진 묘역 철책',group='서쪽 묘역')
for x,y in [(-2480,-680),(-2600,400)]:candles(x,y)
for x,y in [(-2150,2240),(-1900,2260),(-2090,2460)]:
    place('SM_Urn_01',x,y,height=95,yaw=R.randrange(360),group='순례자의 흔적',label='버려진 봉헌 항아리')
place('SM_Metal_Chest',-1760,2240,width=180,yaw=-20,group='순례자의 흔적',label='봉헌 보관함 · 장식')
place('sm_ChapelWall_03_01',-1970,2550,width=950,z=-30,group='순례자의 흔적',label='무너진 봉헌실')

# Subtle low debris reinforces the already-collidable battle islands, never a new obstacle.
for x,y in [(-1250,-500),(1250,600),(-900,1050),(800,-1100)]:
    place('sm_Column_01_01',x,y,width=205,yaw=R.randrange(360),z=-15,label='쓰러진 회랑의 기둥 밑동')
    rubble(x,y,9)

# Existing lighting retains character shading setup; adjust only environment light.
for actor in A.get_all_level_actors():
    label=actor.get_actor_label()
    if label=='숲의 주광원':
        actor.light_component.set_intensity(4.8)
        actor.light_component.set_light_color(u.LinearColor(1.,.86,.69))
        actor.light_component.set_editor_property('light_source_angle',4.)
    elif label=='숲의 보조광':
        actor.light_component.set_intensity(1.8)
        actor.light_component.set_light_color(u.LinearColor(.52,.69,1.))
    elif isinstance(actor,u.PostProcessVolume):
        settings=actor.get_editor_property('settings')
        for key,value in dict(override_color_saturation=True,color_saturation=u.Vector4(.92,.92,.92,1.),
            override_bloom_intensity=True,bloom_intensity=.18).items():settings.set_editor_property(key,value)
        actor.set_editor_property('settings',settings)
assert gameplay_snapshot()==protected_before,'Gameplay geometry changed during art dressing'
report['preserved_gameplay_actors']=protected_before
assert L.save_current_level()
report['status']='SAVED'
report['placement_count']=len(report['placements'])
(OUT/'dressing.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('SANCTUARY ART SAVED: '+str(len(report['placements'])))

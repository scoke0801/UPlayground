"""Art direction: a moss-grown sanctuary, warm canopy light and layered planting."""
import json
import math
import unreal as u

def ground_material(g):
    own,expr,connect,output,constant=[g[n] for n in ['own','expr','connect','output','constant']]
    mat=own('M_PGFR_Ground_Detailed',u.Material,u.MaterialFactoryNew())
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    lib=g['LIB'];lib.delete_all_material_expressions(mat)
    pos=expr(mat,u.MaterialExpressionWorldPosition)
    def custom(code,inputs,kind=u.CustomMaterialOutputType.CMOT_FLOAT1):
        n=expr(mat,u.MaterialExpressionCustom);pins=[]
        for name,source in inputs.items():
            pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
        n.set_editor_property('inputs',pins);n.set_editor_property('output_type',kind);n.set_editor_property('code',code)
        for name,source in inputs.items():connect(source,'',n,name)
        return n
    uv=custom('return P.xy/500.;',{'P':pos},u.CustomMaterialOutputType.CMOT_FLOAT2)
    mask=custom('float n=sin(P.x*.0027+sin(P.y*.0021))*sin(P.y*.0039)+.35*sin(P.x*.009+P.y*.006); float r=length(P.xy); return saturate(.48+n*.33+smoothstep(850.,2100.,r)*.25);',{'P':pos})
    def tex(name,normal=False):
        n=expr(mat,u.MaterialExpressionTextureSample);n.set_editor_property('texture',u.load_asset(g['DEST']+'/Textures/HD_'+name));connect(uv,'',n,'UVs')
        if normal:n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        return n
    def blend(a,b):
        n=expr(mat,u.MaterialExpressionLinearInterpolate);connect(a,'RGB',n,'A');connect(b,'RGB',n,'B');connect(mask,'',n,'Alpha');return n
    color=blend(tex('T_EastLands_ForestGround_01_C'),tex('T_EastLands_ForestGround_02_C'))
    moss_mask=custom('float n=sin(P.x*.0024+sin(P.y*.003))*sin(P.y*.0031); return saturate(smoothstep(900.,2000.,length(P.xy))*.65+n*.25);',{'P':pos})
    green=expr(mat,u.MaterialExpressionLinearInterpolate);connect(color,'',green,'A');connect(tex('T_EastLands_Moss_CR'),'RGB',green,'B');connect(moss_mask,'',green,'Alpha')
    output(green,'',u.MaterialProperty.MP_BASE_COLOR)
    normal=blend(tex('T_EastLands_ForestGround_01_N',True),tex('T_EastLands_ForestGround_02_N',True))
    flat=expr(mat,u.MaterialExpressionConstant3Vector);flat.set_editor_property('constant',u.LinearColor(0,0,1))
    soft=expr(mat,u.MaterialExpressionLinearInterpolate);connect(flat,'',soft,'A');connect(normal,'',soft,'B');connect(constant(mat,.24),'',soft,'Alpha')
    output(soft,'',u.MaterialProperty.MP_NORMAL)
    output(constant(mat,.91),'',u.MaterialProperty.MP_ROUGHNESS)
    output(constant(mat,.18),'',u.MaterialProperty.MP_SPECULAR)
    lib.recompile_material(mat);g['save'](mat)
    return mat

def populate(g):
    rng=g['RNG'];inst=g['instance'];solid=g['solid'];config=g['CONFIG']
    data=json.loads((g['OUT']/'detailed_import.json').read_text(encoding='utf-8'))
    assert data['status']=='PASS'
    for name,row in data['meshes'].items():g['MESHES'][name]=u.load_asset(row['path'])
    g['MESHES']['Ground'].set_material(0,ground_material(g));g['save'](g['MESHES']['Ground'])
    # Visual backdrop extends well beyond every review/game camera; collision stays local.
    inst('Ground',(0,0,-45),(4.,4.,.25),0)
    def dimensions(name):
        row=data['meshes']['HD_'+name]
        return [row['max'][i]-row['min'][i] for i in range(3)]
    def place(name,x,y,width=None,height=None,yaw=0,z=0,scale=None):
        row=data['meshes']['HD_'+name];size=dimensions(name)
        if scale is None:
            scale=width/max(size[:2]) if width is not None else height/size[2] if height is not None else 1.
        if isinstance(scale,(int,float)):scale=(scale,scale,scale)
        # Center the actual geometry, including FBX pivots offset from the object.
        a=math.radians(yaw);cx=(row['max'][0]+row['min'][0])*.5*scale[0];cy=(row['max'][1]+row['min'][1])*.5*scale[1]
        inst('HD_'+name,(x-cx*math.cos(a)+cy*math.sin(a),y-cx*math.sin(a)-cy*math.cos(a),z-row['min'][2]*scale[2]),scale,yaw)
    def paving(name,x,y,width,yaw):
        size=dimensions(name);s=width/max(size[:2])
        place(name,x,y,yaw=yaw,z=.3,scale=(s,s,3/size[2]))
    def pebbles(x,y,width,height):
        size=dimensions('RiverRocks_Cluster_01');s=width/max(size[:2])
        place('RiverRocks_Cluster_01',x,y,yaw=rng.randrange(360),z=-2,scale=(s,s,height/size[2]))
    # Sparse remnants only. Consume the same seeded candidate draws so this floor
    # revision does not reshuffle the already reviewed woodland and landmarks.
    paving_count=0
    for ring in range(8):
        radius=ring*160;count=max(1,round(math.tau*radius/162))
        for i in range(count):
            if ring>5 and rng.random()<.24:continue
            a=i*math.tau/count+ring*.24
            x=math.cos(a)*radius+rng.uniform(-12,12);y=math.sin(a)*radius+rng.uniform(-12,12)
            width=rng.uniform(210,250);yaw=math.degrees(a)+rng.uniform(-12,12)
            remnant=(y<-820 and abs(x)<420) or (x>850 and -180<y<340) or (x<-850 and 220<y<550)
            if remnant and i%2==0:
                paving('FlatStone_01' if i%3 else 'FlatStone_02',x,y,width*.72,yaw)
                paving_count+=1
    for i in range(28):
        y=-1850+i*140;x=math.sin(y*.0015)*260-190
        for j in [-1,0,1]:
            if rng.random()<.15:continue
            yaw=rng.uniform(-30,30)
            if j==0 and i in [0,2,4,23,26]:
                paving('FlatStone_01' if i%2 else 'FlatStone_02',x,y,155,yaw)
                paving_count+=1
    g['REPORT']['paving_remnants']=paving_count
    # Existing battle-island collision shapes; higher-detail visible rocks match them.
    for island in config['combat_islands']:
        x,y=island['center_cm'];s=island['scale'];name=island['mesh']
        z=-g['IMPORTED']['meshes']['SM_PGFR_'+name]['min'][2]*s
        solid(island['name'],name,(x,y,z),s,island['yaw'],visible=False)
        place('MedBoulder_02',x,y,width=240,height=None,yaw=island['yaw'],z=-12)
        for i in range(9):
            a=rng.random()*math.tau;r=rng.uniform(130,260)
            pebbles(x+math.cos(a)*r,y+math.sin(a)*r,rng.uniform(60,130),rng.uniform(12,26))
        for i in range(4):
            a=rng.random()*math.tau
            place('Osmanthus_Bush_01',x+math.cos(a)*175,y+math.sin(a)*175,height=rng.uniform(55,95),yaw=rng.randrange(360))
        for i in range(65):
            a=rng.random()*math.tau;r=rng.uniform(120,310)
            place('SmallGrass_02' if i%4 else 'CirclePlant_01',x+math.cos(a)*r,y+math.sin(a)*r,height=rng.uniform(18,40),yaw=rng.randrange(360))
    hx,hy=config['boundary_half_extent_cm']
    # Continuous visible low masonry, backed by irregular woodland rather than a bare rectangle.
    for axis,half,span in [(0,hx,hy),(1,hy,hx)]:
        for sign in [-1,1]:
            count=round(span*2/240)
            for i in range(count+1):
                t=-span+i*span*2/count;x,y=(sign*half,t) if axis==0 else (t,sign*half)
                place('StoneWall_01' if i%4 else 'StoneWall_02',x,y,width=270,yaw=90 if axis==0 else 0,z=-30)
    # Architectural focal point: one major arch, flanked by sculpted guardian stones.
    place('StoneArch_01',700,-2350,width=1580,yaw=10,z=-10)
    for x in [-150,1450]:
        place('Fox_Statue_Base',x,-2130,width=235,yaw=0)
        place('Fox_Statue',x,-2130,height=265,yaw=0,z=100)
    place('SpiritStatue_01',2420,-700,height=470,yaw=-80)
    place('StoneArch_01',-2650,650,width=1050,yaw=80,z=-110)
    # Cliffs and tree trunks establish asymmetric depth behind the enclosure.
    for i in range(24):
        a=math.tau*i/24;d=rng.uniform(1.10,1.35)
        place('Cliff_01' if i%4==0 else 'BigBoulder_01',math.cos(a)*hx*d,math.sin(a)*hy*d,width=rng.uniform(350,600),yaw=rng.randrange(360),z=-70)
    for i in range(config['tree_count']):
        a=math.tau*i/config['tree_count']+rng.uniform(-.055,.055);d=rng.uniform(*config['tree_ellipse_radius_multiplier'])
        x,y=math.cos(a)*hx*d,math.sin(a)*hy*d
        place(['Osmanthus_01','Osmanthus_02','Conifer_Twisted_01'][i%3],x,y,height=rng.uniform(1050,1850),yaw=rng.randrange(360),z=0)
    # Near-edge trees cast canopy shadows while keeping crowns off the central arena.
    for x,y,h in [(1850,-2650,1250),(-1450,-2550,1400),(2800,900,1150),(-2750,-900,1300)]:
        place('Osmanthus_01',x,y,height=h,yaw=rng.randrange(360))
    for i in range(80):
        a=rng.random()*math.tau;d=rng.uniform(1.06,1.62)
        place('Osmanthus_Bush_01',math.cos(a)*hx*d,math.sin(a)*hy*d,height=rng.uniform(130,300),yaw=rng.randrange(360))
    for i in range(config['grass_cluster_count']):
        x=rng.uniform(-3300,3300);y=rng.uniform(-2900,2900)
        edge=max(abs(x)/hx,abs(y)/hy)
        if edge<.62:continue
        if edge<.90 and rng.random()<.82:continue
        # Irregular foliage fingers, leaving quiet central paving and clear routes.
        if abs(x+190+math.sin(y*.0015)*260)<230 and abs(y)<1900:continue
        kind='SmallGrass_02' if i%5 else 'MediumGrass_02'
        if i%17==0:kind='LargeLeaves_01'
        if i%29==0:kind='Iris_01'
        if i%41==0:kind='FlowerGroup_01'
        place(kind,x,y,height=rng.uniform(20,55) if edge<1 else rng.uniform(35,85),yaw=rng.randrange(360))
    for i in range(100):
        a=rng.random()*math.tau;r=rng.uniform(1450,2200)
        pebbles(math.cos(a)*r,math.sin(a)*r*.86,rng.uniform(30,75),rng.uniform(5,12))
    for x,y in [(-1950,-1000),(1850,1200),(-1400,1710)]:place('Log_Large',x,y,width=400,yaw=rng.randrange(360),z=-12)
    for x,y in [(-550,-1820),(450,-1800),(-1780,1200),(1750,1350),(-1850,-1250),(1890,-950)]:
        place('Lamp_Ground_01',x,y,height=155,yaw=rng.randrange(360))
        light=g['ACTORS'].spawn_actor_from_class(u.PointLight,u.Vector(x,y,115));light.set_actor_label('성소의 따뜻한 등불');light.set_folder_path('조명')
        light.light_component.set_mobility(u.ComponentMobility.MOVABLE);light.light_component.set_intensity(45)
        light.light_component.set_light_color(u.LinearColor(1.,.42,.12));light.light_component.set_editor_property('attenuation_radius',340)
        light.light_component.set_cast_shadows(False)
    g['REPORT']['art_direction']='Moss-grown sanctuary / EastLands standalone LOD0 / layered woodland'
    g['REPORT']['detailed_meshes']=data['meshes']

def atmosphere(g):
    cube_path='/Engine/MapTemplates/Sky/DaylightAmbientCubemap'
    cube=u.load_asset(cube_path)
    if cube:
        sky=g['ACTORS'].spawn_actor_from_class(u.SkyLight,u.Vector(0,0,1500))
        sky.set_actor_label('수관 아래 푸른 환경광');sky.set_folder_path('조명')
        comp=sky.light_component;comp.set_mobility(u.ComponentMobility.MOVABLE)
        comp.set_editor_property('source_type',u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        comp.set_editor_property('cubemap',cube);comp.set_intensity(.65)
        comp.set_light_color(u.LinearColor(.78,.88,1.))
        g['REPORT']['ambient_cubemap']=cube_path
    sky=g['ACTORS'].spawn_actor_from_class(u.SkyAtmosphere,u.Vector(0,0,-100))
    sky.set_actor_label('성소의 하늘');sky.set_folder_path('조명')
    actor=g['ACTORS'].spawn_actor_from_class(u.ExponentialHeightFog,u.Vector(0,0,-250))
    actor.set_actor_label('숲의 깊이 안개');actor.set_folder_path('조명')
    fog=actor.component
    fog.set_editor_property('fog_density',.008)
    fog.set_editor_property('fog_height_falloff',.3)
    fog.set_editor_property('fog_max_opacity',.32)
    fog.set_editor_property('start_distance',2600.)
    fog.set_editor_property('fog_inscattering_luminance',u.LinearColor(.12,.20,.22))

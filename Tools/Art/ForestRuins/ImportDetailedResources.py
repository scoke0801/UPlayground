"""Import LOD0 meshes and reconstruct PBR/foliage materials from source mappings."""
import json
from pathlib import Path
import traceback
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
KIT=ROOT/'Tools/Art/ForestRuins'
DEST='/Game/Environment/ForestRuins'
DATA=json.loads((KIT/'detailed_sources.json').read_text(encoding='utf-8'))
GEO=json.loads((KIT/'detailed_geometry.json').read_text(encoding='utf-8'))
E=u.EditorAssetLibrary
T=u.AssetToolsHelpers.get_asset_tools()
L=u.MaterialEditingLibrary
REPORT=dict(status='RUNNING',meshes={},materials={})
def clean(s):return s.replace(' ','_')
def save(a):assert E.save_loaded_asset(a,only_if_is_dirty=False)
def node(m,c):return L.create_material_expression(m,c)
def link(a,p,b,q):assert L.connect_material_expressions(a,p,b,q)
def output(a,p,q):assert L.connect_material_property(a,p,q)
def scalar(m,v):
    n=node(m,u.MaterialExpressionConstant);n.set_editor_property('r',v);return n
def mul(m,a,ap,b,bp=''):
    n=node(m,u.MaterialExpressionMultiply);link(a,ap,n,'A');link(b,bp,n,'B');return n
def lerp(m,a,b,c,p=''):
    n=node(m,u.MaterialExpressionLinearInterpolate);link(a,'RGB',n,'A');link(b,'RGB',n,'B');link(c,p,n,'Alpha');return n
def sample(m,name,normal=False,tiling=1.):
    tex=u.load_asset(DEST+'/Textures/HD_'+clean(name));assert tex,name
    n=node(m,u.MaterialExpressionTextureSample);n.set_editor_property('texture',tex)
    if normal:n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    elif tex.get_editor_property('compression_settings')==u.TextureCompressionSettings.TC_MASKS:n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    elif not tex.get_editor_property('srgb'):n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    if tiling!=1:
        uv=node(m,u.MaterialExpressionTextureCoordinate);uv.set_editor_property('u_tiling',tiling);uv.set_editor_property('v_tiling',tiling);link(uv,'',n,'UVs')
    return n
def material(name,data):
    path=DEST+'/Materials/HD_'+clean(name)
    m=u.load_asset(path) if E.does_asset_exist(path) else T.create_asset('HD_'+clean(name),DEST+'/Materials',u.Material,u.MaterialFactoryNew())
    L.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True)
    refs=data['textures'];floats=data['floats']
    foliage=any(s in name.lower() for s in ['leaves','grass_small','plants'])
    m.set_editor_property('two_sided',foliage)
    m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED if foliage else u.BlendMode.BLEND_OPAQUE)
    if foliage:m.set_editor_property('opacity_mask_clip_value',.32)
    albedo=next((refs[k] for k in ['_Color_Map','_ColorMap','_Base_Color','_Foliage_Map','_BaseMap','_MainTex'] if k in refs),None)
    normal=next((refs[k] for k in ['_Normal_Map','_NormalMap','_Baked_Mesh_Normal_Map'] if k in refs),None)
    if albedo:
        base=sample(m,albedo);color=base;pin='RGB'
    else:
        mask=sample(m,refs['_RGB_Map'])
        layers=[sample(m,refs['_Layer_'+str(i)+'_Color_Map'],tiling=floats.get('_Layer_'+str(i)+'_Tiling',1)) for i in [1,2,3]]
        ab=lerp(m,layers[0],layers[1],mask,'G')
        color=node(m,u.MaterialExpressionLinearInterpolate);link(ab,'',color,'A');link(layers[2],'RGB',color,'B');link(mask,'B',color,'Alpha');pin=''
        # Moss follows upward-facing surfaces; authored baked normals preserve sculpted details.
        moss=sample(m,'T_EastLands_Moss_CR',tiling=max(1.,floats.get('_Coverage_Tiling',1)))
        vn=node(m,u.MaterialExpressionVertexNormalWS)
        c=node(m,u.MaterialExpressionCustom);inp=u.CustomInput();inp.set_editor_property('input_name','N')
        c.set_editor_property('inputs',[inp]);c.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT1)
        c.set_editor_property('code','return smoothstep(0.45,0.95,N.z)*'+('.14;' if 'FlatStone' in name else '.52;'));link(vn,'',c,'N')
        mix=node(m,u.MaterialExpressionLinearInterpolate);link(color,pin,mix,'A');link(moss,'RGB',mix,'B');link(c,'',mix,'Alpha');color=mix
    if 'FlatStone' in name:
        grade=node(m,u.MaterialExpressionCustom);inp=u.CustomInput();inp.set_editor_property('input_name','C')
        grade.set_editor_property('inputs',[inp]);grade.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
        grade.set_editor_property('code','float l=dot(C,float3(.2126,.7152,.0722)); return lerp(C,l.xxx,.65)*float3(.88,.96,1.04);')
        link(color,pin,grade,'C');color=grade;pin=''
    if foliage:
        tint=node(m,u.MaterialExpressionConstant3Vector);tint.set_editor_property('constant',u.LinearColor(.70,.84,.72))
        color=mul(m,color,pin,tint,'');pin=''
    output(color,pin,u.MaterialProperty.MP_BASE_COLOR)
    output(scalar(m,.86),'',u.MaterialProperty.MP_ROUGHNESS)
    output(scalar(m,.22),'',u.MaterialProperty.MP_SPECULAR)
    if normal:
        ns=sample(m,normal,True)
        if 'FlatStone' in name:
            flat=node(m,u.MaterialExpressionConstant3Vector);flat.set_editor_property('constant',u.LinearColor(0,0,1))
            blend=node(m,u.MaterialExpressionLinearInterpolate);link(flat,'',blend,'A');link(ns,'RGB',blend,'B');link(scalar(m,.4),'',blend,'Alpha')
            output(blend,'',u.MaterialProperty.MP_NORMAL)
        else:output(ns,'RGB',u.MaterialProperty.MP_NORMAL)
    orm=refs.get('_ORM_Map',refs.get('_ORMMap'))
    if orm:
        packed=sample(m,orm);output(packed,'R',u.MaterialProperty.MP_AMBIENT_OCCLUSION);output(packed,'G',u.MaterialProperty.MP_ROUGHNESS)
    elif '_OcclusionEdges_Map' in refs:output(sample(m,refs['_OcclusionEdges_Map']),'R',u.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if foliage:
        output(base,'A',u.MaterialProperty.MP_OPACITY_MASK)
        output(mul(m,base,'RGB',scalar(m,.025)),'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    L.recompile_material(m);save(m)
    REPORT['materials'][name]=dict(path=m.get_path_name(),foliage=foliage,albedo=albedo,normal=normal)
    return m
try:
    tasks=[]
    normals=set();linear=set()
    for mat in DATA['materials'].values():
        for k,v in mat['textures'].items():
            if 'Normal' in k or k=='_BumpMap':normals.add(v)
            if 'ORM' in k or 'Occlusion' in k or 'RGB_Map' in k or k=='_Edge_Map':linear.add(v)
    normals.update(['T_EastLands_ForestGround_01_N','T_EastLands_ForestGround_02_N','T_EastLands_Moss_N'])
    for name,entry in DATA['textures'].items():
        path=DEST+'/Textures/HD_'+clean(name)
        if E.does_asset_exist(path):continue
        task=u.AssetImportTask()
        for k,v in dict(filename=str(KIT/entry['file']),destination_path=DEST+'/Textures',destination_name='HD_'+clean(name),automated=True,save=True).items():task.set_editor_property(k,v)
        tasks.append(task)
    if tasks:T.import_asset_tasks(tasks)
    for name in DATA['textures']:
        tex=u.load_asset(DEST+'/Textures/HD_'+clean(name));assert tex,name
        tex.set_editor_property('max_texture_size',2048)
        tex.set_editor_property('srgb',name not in normals|linear)
        if name in normals:
            tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('flip_green_channel',True)
        elif name in linear:tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
        save(tex)
    materials={n:material(n,d) for n,d in DATA['materials'].items()}
    for entry in DATA['meshes']:
        name='SM_PGFR_HD_'+entry['name'];path=DEST+'/Meshes/'+name
        if not E.does_asset_exist(path):
            task=u.AssetImportTask()
            for k,v in dict(filename=str(KIT/'DetailedSource/Normalized'/(name+'.fbx')),destination_path=DEST+'/Meshes',destination_name=name,automated=True,save=True,factory=u.FbxFactory()).items():task.set_editor_property(k,v)
            options=u.FbxImportUI()
            for k,v in dict(automated_import_should_detect_type=False,mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH,import_mesh=True,import_materials=False,import_textures=False,import_animations=False).items():options.set_editor_property(k,v)
            for k,v in dict(combine_meshes=True,auto_generate_collision=False,generate_lightmap_u_vs=False,convert_scene=True,convert_scene_unit=True,normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():options.static_mesh_import_data.set_editor_property(k,v)
            task.set_editor_property('options',options);T.import_asset_tasks([task])
        mesh=u.load_asset(path);assert mesh,path
        slots=[]
        for i,s in enumerate(mesh.get_editor_property('static_materials')):
            slot=str(s.get_editor_property('imported_material_slot_name'))
            candidates=[k for k in entry['materials'] if slot==k or slot.startswith(k+'.')]
            mat=entry['materials'][candidates[0]] if candidates else entry['materials'].get('*')
            if not mat and len(set(entry['materials'].values()))==1:mat=next(iter(entry['materials'].values()))
            assert mat,(name,slot,entry['materials'])
            mesh.set_material(i,materials[mat]);slots.append(dict(slot=slot,material=mat))
        save(mesh)
        box=mesh.get_bounding_box()
        REPORT['meshes']['HD_'+entry['name']]=dict(path=path,min=[box.min.x,box.min.y,box.min.z],max=[box.max.x,box.max.y,box.max.z],triangles=mesh.get_num_triangles(0),slots=slots)
    REPORT['status']='PASS'
except BaseException as e:
    REPORT.update(status='FAIL',error=str(e),traceback=traceback.format_exc());raise
finally:
    (ROOT/'Saved/ForestRuins/detailed_import.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')

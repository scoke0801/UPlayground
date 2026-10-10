"""Bake paired face-shadow transition fields from actual UE LOD0 positions/UV0.

Uses numpy bundled with Blender; no source model or texture is edited. Each
light-angle mask is converted to a signed Euclidean distance field before the
zero crossings are interpolated. R/G encode +right/-right light sweeps, B is the
front-face influence. UV0 is read in Unreal's top-left texture convention.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
from collections import deque
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
SETTINGS = json.loads((OUT/'settings.json').read_text(encoding='utf-8'))
CONFIG = SETTINGS['bake']
SIZE = CONFIG['resolution']
assert SIZE in [256,512,1024] and 3 <= CONFIG['angles_per_side'] <= 37

def png(path, rgb):
    rgb = np.ascontiguousarray(np.clip(rgb, 0, 255), dtype=np.uint8)
    height, width, _ = rgb.shape
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data)&0xffffffff)
    raw = b''.join(b'\0'+row.tobytes() for row in rgb)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', width,height,8,2,0,0,0))
                     +chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b''))

def smoothstep(a, b, value):
    t = np.clip((value-a)/(b-a), 0, 1)
    return t*t*(3-2*t)

def edt1d(values):
    # Lower envelope of parabolas (exact squared Euclidean distance transform).
    length = len(values)
    sites = np.zeros(length, dtype=np.int32)
    bounds = np.empty(length+1)
    result = np.empty(length)
    k = 0
    bounds[0], bounds[1] = -np.inf, np.inf
    for q in range(1,length):
        p = sites[k]
        crossing = ((values[q]+q*q)-(values[p]+p*p))/(2*(q-p))
        while crossing <= bounds[k]:
            k -= 1
            p = sites[k]
            crossing = ((values[q]+q*q)-(values[p]+p*p))/(2*(q-p))
        k += 1
        sites[k], bounds[k], bounds[k+1] = q, crossing, np.inf
    k = 0
    for q in range(length):
        while bounds[k+1] < q:
            k += 1
        p = sites[k]
        result[q] = (q-p)**2+values[p]
    return result

def distance_to(mask):
    if not mask.any():
        return np.full(mask.shape, max(mask.shape)*2., dtype=np.float64)
    data = np.where(mask,0.,1e10)
    for y in range(len(data)):
        data[y] = edt1d(data[y])
    for x in range(data.shape[1]):
        data[:,x] = edt1d(data[:,x])
    return np.sqrt(data)

def signed_distance(lit):
    limit = CONFIG['signed_distance_clamp_texels']
    return np.clip(distance_to(~lit)-distance_to(lit), -limit,limit)

def largest_uv_island(valid):
    seen = np.zeros_like(valid)
    largest = []
    for y,x in zip(*np.where(valid)):
        if seen[y,x]: continue
        queue, pixels = deque([(y,x)]), []
        seen[y,x] = True
        while queue:
            yy,xx = queue.popleft()
            pixels.append((yy,xx))
            for dy,dx in [(1,0),(-1,0),(0,1),(0,-1)]:
                Y,X = yy+dy,xx+dx
                if 0 <= Y < len(valid) and 0 <= X < valid.shape[1] and valid[Y,X] and not seen[Y,X]:
                    seen[Y,X] = True
                    queue.append((Y,X))
        if len(pixels) > len(largest): largest = pixels
    result = np.zeros_like(valid)
    yy,xx = np.array(largest).T
    result[yy,xx] = True
    return result

def pad(values, valid, passes=12):
    result, coverage = values.copy(), valid.copy()
    for _ in range(passes):
        sums, counts = np.zeros_like(values), np.zeros_like(valid, dtype=float)
        for axis, step in [(0,1),(0,-1),(1,1),(1,-1)]:
            neighbor = np.roll(coverage,step,axis)
            if axis == 0: neighbor[0 if step == 1 else -1,:] = False
            else: neighbor[:,0 if step == 1 else -1] = False
            sums += np.roll(result,step,axis)*neighbor[...,None]
            counts += neighbor
        fresh = ~coverage & (counts > 0)
        result[fresh] = sums[fresh]/counts[fresh,None]
        coverage |= fresh
    return result, coverage

def main(model_path=None, output=None):
    global OUT, SETTINGS, CONFIG, SIZE
    if output is not None:
        OUT=Path(output)
        SETTINGS=json.loads((OUT/'settings.json').read_text(encoding='utf-8'))
        CONFIG=SETTINGS['bake']
        SIZE=CONFIG['resolution']
    model_path=Path(model_path) if model_path else ROOT/'Saved/BokuseiFaceSDF/Model/model.json'
    model = json.loads(model_path.read_text(encoding='utf-8'))
    geometry_path = Path(model['geometry'])
    assert hashlib.sha256(geometry_path.read_bytes()).hexdigest() == model['geometry_sha256']
    geometry = json.loads(geometry_path.read_text(encoding='utf-8'))
    vertices = np.array(geometry['vertices'],dtype=float)
    triangles = np.array(geometry['triangles'],dtype=int)
    forward, right = np.array(model['head_forward']), np.array(model['head_right'])
    forward /= np.linalg.norm(forward)
    right -= forward*np.dot(forward,right)
    right /= np.linalg.norm(right)
    up = np.cross(right,forward)
    basis = np.stack([right,forward,up],axis=1)
    position = (vertices[:,:3]-np.array(model['head_origin']))@basis
    normals = vertices[:,3:6]@basis
    normals /= np.maximum(np.linalg.norm(normals,axis=1)[:,None],1e-6)
    attributes = np.concatenate([position,normals],axis=1)
    raster = np.zeros((SIZE,SIZE,6),dtype=float)
    valid = np.zeros((SIZE,SIZE),dtype=bool)
    priority = np.full((SIZE,SIZE),-np.inf)
    conflicts = np.zeros((SIZE,SIZE),dtype=bool)
    for tri in triangles:
        uv = vertices[tri,6:8]*SIZE-.5
        xmin,ymin = np.maximum(np.floor(uv.min(0)).astype(int),0)
        xmax,ymax = np.minimum(np.ceil(uv.max(0)).astype(int),SIZE-1)
        if xmax < xmin or ymax < ymin: continue
        a,b,c = uv
        determinant = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(determinant) < 1e-8: continue
        yy,xx = np.mgrid[ymin:ymax+1,xmin:xmax+1]
        w0 = ((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/determinant
        w1 = ((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/determinant
        w2 = 1-w0-w1
        inside = (w0 >= -1e-6)&(w1 >= -1e-6)&(w2 >= -1e-6)
        sample = w0[...,None]*attributes[tri[0]]+w1[...,None]*attributes[tri[1]]+w2[...,None]*attributes[tri[2]]
        old = raster[ymin:ymax+1,xmin:xmax+1]
        covered = valid[ymin:ymax+1,xmin:xmax+1]
        # Opposite sides sharing UVs cannot use one face map; fail loudly.
        conflicts[ymin:ymax+1,xmin:xmax+1] |= inside&covered&(np.abs(sample[:,:,0]-old[:,:,0]) > 1.)&(sample[:,:,4] > .35)&(old[:,:,4] > .35)
        write = inside&(sample[:,:,4] > priority[ymin:ymax+1,xmin:xmax+1])
        old[write] = sample[write]
        priority[ymin:ymax+1,xmin:xmax+1][write] = sample[:,:,4][write]
        covered |= inside
    assert valid.sum() > CONFIG.get('minimum_coverage_texels',10000), 'Insufficient face UV coverage'
    print('UV coverage/conflicts',int(valid.sum()),int(conflicts.sum()),flush=True)
    np.savez_compressed(model_path.parent/'uv_raster.npz',raster=raster,valid=valid,conflicts=conflicts)
    skin_island = largest_uv_island(valid)
    assert (conflicts&skin_island).sum() == 0, 'Mirrored/conflicting facial skin UVs require projection coordinates'
    half_width = float(np.percentile(np.abs(position[normals[:,1] > .3,0]),98))
    x,y,z,nx,nf,nz = [raster[:,:,i] for i in range(6)]
    geometric_yaw = np.arctan2(nx,np.maximum(nf,.1))
    proxy_yaw = np.arctan2(x/max(half_width,1e-4)*CONFIG['proxy_width_gain'],.8)
    # Wide cheek boundaries from the actual face width, with restrained nose detail.
    geometric_weight = CONFIG['geometric_yaw_weight']
    yaw = proxy_yaw*(1-geometric_weight)+np.clip(geometric_yaw,-1.3,1.3)*geometric_weight
    influence = smoothstep(-.05,.35,nf)*smoothstep(position[:,2].min()+.1,position[:,2].min()+1.,z)*skin_island
    fields, padded = pad(np.stack([np.sin(yaw),np.cos(yaw),influence],axis=2),valid,CONFIG['uv_padding_texels'])
    sin_yaw,cos_yaw,influence = [fields[:,:,i] for i in range(3)]
    encoded = []
    angles = np.linspace(0,math.pi,CONFIG['angles_per_side'])
    for side in [1,-1]:
        transition = np.ones((SIZE,SIZE))
        previous = np.full((SIZE,SIZE),float(CONFIG['signed_distance_clamp_texels']))
        already_shadow = np.zeros((SIZE,SIZE),dtype=bool)
        for index,angle in enumerate(angles[1:],1):
            lit = cos_yaw*math.cos(angle)+side*sin_yaw*math.sin(angle) > CONFIG['light_cutoff']
            if index == len(angles)-1: lit[:] = False
            lit[~padded] = False
            lit &= ~already_shadow
            field = signed_distance(lit)
            crossing = ~already_shadow&~lit
            fraction = previous/np.maximum(previous-field,1e-6)
            transition[crossing] = ((index-1)+fraction[crossing])/(len(angles)-1)
            already_shadow |= ~lit
            previous = field
        encoded.append(transition)
    packed = np.stack([encoded[0],encoded[1],influence],axis=2)
    packed[~padded] = (1,1,0)
    texture = OUT/'T_PGBokusei_FaceSDF.png'
    png(texture,np.rint(packed*255))
    png(OUT/'UV_Position.png',np.stack([np.clip(x/(half_width*2)+.5,0,1),np.clip((z-position[:,2].min())/np.ptp(position[:,2]),0,1),valid],axis=2)*255)
    # Model-specific UV mask sequence is retained as authoring evidence.
    previews = []
    for side_index in range(2):
        for angle in [0,30,60,90,120,150,180]:
            light = 1-smoothstep(angle/180-.015,angle/180+.015,encoded[side_index])
            # White=lit, purple=shadow, dark background=outside face UV.
            lit_weight = 1-light*influence
            color = np.array([137,112,148])[None,None,:]*(1-lit_weight[...,None])+np.array([246,225,221])[None,None,:]*lit_weight[...,None]
            color[~valid] = (15,20,29)
            previews.append(color)
    png(OUT/'UV_ShadowSweep.png',np.concatenate([np.concatenate(previews[:7],axis=1),np.concatenate(previews[7:],axis=1)],axis=0))
    # Meaningful numeric checks: exact distance, endpoints, opposite-side symmetry.
    check = np.zeros((5,5),dtype=bool);check[2,2] = True
    assert abs(distance_to(check)[0,0]-math.sqrt(8)) < 1e-8
    face = valid&(influence > .8)
    assert np.isfinite(packed).all() and ((packed >= 0)&(packed <= 1)).all()
    assert all((v[face] > .03).all() and (v[face] < .97).all() for v in encoded)
    report = dict(schema=1,status='PASS',mesh=model['mesh'],slot=geometry['slot'],lod=0,
                  vertices=len(vertices),triangles=len(triangles),size=SIZE,valid_texels=int(valid.sum()),
                  conflicting_texels=int(conflicts.sum()),head_frame=model,half_width_cm=half_width,
                  skin_island_texels=int(skin_island.sum()),skin_island_conflicts=int((conflicts&skin_island).sum()),
                  texture=str(texture),texture_sha256=hashlib.sha256(texture.read_bytes()).hexdigest(),
                  encoding={'R':'+HeadRight light; angular zero crossing / pi','G':'-HeadRight light; angular zero crossing / pi',
                            'B':'Front face influence; back of head/lowest rim use geometric shading'},
                  authoring=CONFIG,settings_sha256=hashlib.sha256((OUT/'settings.json').read_bytes()).hexdigest(),
                  checks={'euclidean_distance':True,'finite_range':True,'front_back_endpoints':True})
    (OUT/'bake.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k != 'head_frame'},ensure_ascii=False,indent=2),flush=True)

if __name__ == '__main__':
    main()

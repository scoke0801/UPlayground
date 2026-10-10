"""Headless Blender: three coarse convex forelock volumes from actual Bokusei hair."""
import json
from pathlib import Path
import bmesh
import numpy as np

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Tools/Art/ToonTest/HairShadowProxy'
OUT.mkdir(parents=True,exist_ok=True)
model=next(m for m in json.loads((ROOT/'Saved/ToonImprovement/Inventory/inventory.json').read_text())['characters'] if m['id']=='Bokusei')
origin=np.asarray(model['head_origin'])
f=np.asarray(model['head_forward']);f/=np.linalg.norm(f)
r=np.asarray(model['head_right']);r-=f*np.dot(f,r);r/=np.linalg.norm(r)
basis=np.stack([r,f,np.cross(r,f)],axis=1)
source=json.loads(Path(model['materials'][4]['geometry']).read_text())
v=(np.asarray(source['vertices'])[:,:3]-origin)@basis
# Broad source-derived clumps; exclude collar-length hair and the sides of the
# face. No alpha cards or duplicate full skeleton are needed for this occluder.
settings=dict(min_height_cm=8.,min_front_cm=1.,inset_cm=.25,bins=[-12.,-3.,3.,12.],grid_cm=1.2)
vertices=[];triangles=[]
for lo,hi in zip(settings['bins'][:-1],settings['bins'][1:]):
    points=v[(v[:,0]>=lo)&(v[:,0]<=hi)&(v[:,1]>=settings['min_front_cm'])&(v[:,2]>=settings['min_height_cm'])]
    grid=np.round(points/settings['grid_cm']).astype(int)
    _,indices=np.unique(grid,axis=0,return_index=True)
    points=points[indices].copy()
    points[:,1]-=settings['inset_cm']
    assert len(points)>=4
    bm=bmesh.new()
    for p in points:bm.verts.new(p.tolist())
    result=bmesh.ops.convex_hull(bm,input=list(bm.verts),use_existing_faces=False)
    discard=set(result.get('geom_unused',[])+result.get('geom_interior',[]))
    if discard:bmesh.ops.delete(bm,geom=list(discard),context='VERTS')
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    bm.verts.ensure_lookup_table();bm.verts.index_update()
    offset=len(vertices)
    # Store component-space positions. Unreal converts to exact head-bone local
    # space when building the asset, avoiding FBX axis/centimeter conversions.
    vertices.extend((np.asarray([list(x.co) for x in bm.verts])@basis.T+origin).tolist())
    triangles.extend([[offset+x.index for x in face.verts] for face in bm.faces])
    bm.free()
assert 0<len(triangles)<600
report=dict(status='PASS',model='Bokusei',settings=settings,head_transform=model['head_transform'],vertices=vertices,triangles=triangles)
(OUT/'proxy.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('Hair proxy triangles:',len(triangles))

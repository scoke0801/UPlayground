"""Exact separable EDT; vectorized across independent scanlines with NumPy."""
import numpy as np


def rows(values):
    count,length=values.shape
    ids=np.arange(count)
    sites=np.zeros((count,length),dtype=np.int32)
    bounds=np.full((count,length+1),np.inf)
    bounds[:,0]=-np.inf
    k=np.zeros(count,dtype=np.int32)
    for q in range(1,length):
        while True:
            p=sites[ids,k]
            crossing=((values[:,q]+q*q)-(values[ids,p]+p*p))/(2*(q-p))
            pop=crossing<=bounds[ids,k]
            if not pop.any():break
            k[pop]-=1
        k+=1
        sites[ids,k]=q
        bounds[ids,k]=crossing
        bounds[ids,k+1]=np.inf
    k[:]=0
    result=np.empty_like(values)
    for q in range(length):
        while True:
            advance=bounds[ids,k+1]<q
            if not advance.any():break
            k[advance]+=1
        p=sites[ids,k]
        result[:,q]=(q-p)**2+values[ids,p]
    return result


def distance_to(mask):
    if not mask.any():return np.full(mask.shape,max(mask.shape)*2.,dtype=np.float64)
    values=np.where(mask,0.,1e10)
    return np.sqrt(rows(rows(values).T).T)


def verify():
    # Compare against brute force distances, including empty/full, asymmetric and
    # rectangular fields. This tests the numerical contract, not shader source text.
    rng=np.random.default_rng(219)
    for shape in [(5,5),(9,13),(16,7)]:
        for density in [0.,.05,.5,1.]:
            mask=rng.random(shape)<density
            seeds=np.argwhere(mask)
            expected=np.full(shape,max(shape)*2.,dtype=float)
            if len(seeds):
                points=np.indices(shape).transpose(1,2,0)
                expected=np.sqrt(((points[:,:,None,:]-seeds[None,None,:,:])**2).sum(-1).min(-1))
            assert np.allclose(distance_to(mask),expected,rtol=0,atol=1e-10),(shape,density)


if __name__=='__main__':verify();print('Exact face distance field: PASS')

"""Read lossless UE captures without image dependencies; gate cold first cast and build differences.
This measures the isolated fixture only; it does not replace visual review or gameplay acceptance.
"""
import argparse
import json
from pathlib import Path
import struct
import zlib

def pixels(path):
    data=path.read_bytes(); assert data[:8]==b'\x89PNG\r\n\x1a\n'
    width,height,depth,color,_,_,interlace=struct.unpack('>IIBBBBB',data[16:29])
    assert depth==8 and color in (2,6) and interlace==0
    channels=4 if color==6 else 3
    stream=bytearray(); offset=8
    while offset<len(data):
        size=struct.unpack('>I',data[offset:offset+4])[0];kind=data[offset+4:offset+8]
        if kind==b'IDAT':stream.extend(data[offset+8:offset+8+size])
        offset+=size+12
    raw=zlib.decompress(stream);stride=width*channels;prev=bytearray(stride);rows=[]
    for y in range(height):
        start=y*(stride+1);filter_type=raw[start];row=bytearray(raw[start+1:start+stride+1])
        for x in range(stride):
            a=row[x-channels] if x>=channels else 0;b=prev[x];c=prev[x-channels] if x>=channels else 0
            if filter_type==1:predict=a
            elif filter_type==2:predict=b
            elif filter_type==3:predict=(a+b)//2
            elif filter_type==4:
                p=a+b-c;pa=abs(p-a);pb=abs(p-b);pc=abs(p-c)
                predict=a if pa<=pb and pa<=pc else b if pb<=pc else c
            else:
                assert filter_type==0;predict=0
            row[x]=(row[x]+predict)&255
        rows.append(row);prev=row
    return width,height,channels,rows

def first_cast(path):
    width,height,channels,rows=pixels(path)
    assert (width,height)==(1280,720)
    # Below the fixture's player; excludes the white hair/body and pink impact particles.
    bright=[]
    for y in range(400,500):
        for x in range(520,790):
            rgb=tuple(rows[y][x*channels:x*channels+3])
            if min(rgb)>185:bright.append(rgb)
    return dict(visible_pixels=len(bright),pass_visible=len(bright)>350)

def build_color(path):
    _,_,channels,rows=pixels(path)
    values=[]
    for y in range(400,500):
        for x in range(520,790):
            rgb=tuple(rows[y][x*channels:x*channels+3])
            if max(rgb)>150 and max(rgb)-min(rgb)>35:values.append(rgb)
    return dict(colored_pixels=len(values),rgb=[round(sum(v[i] for v in values)/max(1,len(values)),1) for i in range(3)])

def main():
    parser=argparse.ArgumentParser();parser.add_argument('run',type=Path);args=parser.parse_args()
    base=args.run/'render_b0_p0/User/Saved/QA/HackSlashP0/Skill_100.png'
    report={'first_cast':first_cast(base),'builds':{}}
    for build in range(1,5):
        path=args.run/f'render_b{build}_p0/User/Saved/QA/HackSlashP0/Skill_100_Miss.png'
        if path.exists():report['builds'][str(build)]=build_color(path)
    errors=[]
    if not report['first_cast']['pass_visible']:errors.append('First cast is not visibly rendered')
    if len(report['builds'])==4:
        for build,channel in [('1',0),('2',2),('3',0)]:
            sample=report['builds'][build];rgb=sample['rgb']
            if sample['colored_pixels']<60:errors.append('Insufficient colored build pixels '+build)
            if rgb[channel]<=min(rgb)+30:errors.append('Missing build hue '+build)
    report['errors']=errors;report['status']='FAIL' if errors else 'PASS'
    (args.run/'pixel-review.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2));return bool(errors)
if __name__=='__main__':raise SystemExit(main())

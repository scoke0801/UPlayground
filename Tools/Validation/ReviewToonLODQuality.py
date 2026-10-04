"""Verify all frames and encode annotated comparisons; never infer visual PASS."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'Saved/ToonTest/QualityTools'))
import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageDraw, ImageFont

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--mode', choices=['comparison','close','sweep'], help='Rebuild only one video group')
    parser.add_argument('--trim-first', type=int, default=0,
                        help='Explicitly exclude this many leading frames of the first phase; raw images stay unchanged')
    args = parser.parse_args()
    out = args.run.resolve()
    capture = json.loads((out/'capture.json').read_text())
    assert capture['status'] == 'CAPTURED'
    if args.mode:
        capture['plan'] = [p for p in capture['plan'] if p['mode'] == args.mode]
        names = {p['name'] for p in capture['plan']}
        capture['frames'] = [r for r in capture['frames'] if r['phase'] in names]
        assert capture['plan'], 'Requested mode is absent'
    w, h = capture['viewport']
    fps = capture['options']['fps']
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', max(16, h//36))
    report = dict(status='FRAME_VALIDATED', resolution=[w,h], phases=[], transitions=[], videos=[],
                  visual_status='REVIEW_REQUIRED', limitation='Fixed-step editor capture; not measured real-time performance')
    report['excluded_leading_frames'] = args.trim_first
    rows = capture['frames'][args.trim_first:]
    phases = {p['name']: p for p in capture['plan']}
    previous = {}
    for name, phase in phases.items():
        frames = [r for r in rows if r['phase'] == name]
        offset = args.trim_first if name == capture['plan'][0]['name'] else 0
        assert len(frames) == phase['frames']-offset, (name,len(frames))
        differences, unique, timestamps, hands, lods = [], set(), [], [], []
        samples = []
        for i,row in enumerate(frames):
            path = out/name/f"{row['frame']:05d}.png"
            im = Image.open(path).convert('RGB')
            assert im.size == (w,h), path
            unique.add(hashlib.sha256(path.read_bytes()).hexdigest())
            timestamps.append(row['rendered_world_time'])
            hands.append(row['rendered_pose'][1]['hand'])
            current_lod = row['rendered_pose'][1]['lod']
            lods.append(current_lod)
            if phase['mode'] == 'comparison':
                assert [p['lod'] for p in row['rendered_pose']] == [0,1,2]
            if phase['mode'] == 'close':
                assert current_lod == (0 if name == 'shadow_lod0' else 1)
            if 'shadow_cache' in row and not (capture['options'].get('diagnostic') or capture['options'].get('shadow_check')):
                assert row['shadow_cache'] == 1, 'Normal capture must retain VSM caching'
            if phase['mode'] == 'sweep' and i and lods[-2] != current_lod:
                report['transitions'].append(dict(phase=name, frame=i, time=i/fps,
                    distance_cm=row['distance_cm'], before=lods[-2], after=current_lod))
            # Whole character crop: rank temporal changes for manual review.
            # Motion contributes; this number is NOT a flicker detector or pass criterion.
            crop = np.asarray(im.crop((w//3,h//5,2*w//3,4*h//5)), dtype=np.float32)
            if name in previous:
                differences.append((float(np.abs(crop-previous[name]).mean()), row['frame']))
            previous[name] = crop
            if i in [0, 15, 30, 45, 60, 75, 90, 105] or (phase['mode']=='sweep' and i%60==0):
                samples.append((i,im.copy()))
        deltas = np.diff(timestamps)
        assert np.allclose(deltas, 1/fps, atol=.0001), (name,float(deltas.min()),float(deltas.max()))
        assert len(unique) > len(frames)*.8, 'Too many identical frames: '+name
        assert np.max(np.ptp(np.asarray(hands),axis=0)) > .001, 'No bone movement: '+name
        report['phases'].append(dict(name=name, frames=len(frames), unique_frames=len(unique),
            selected_lods=sorted(set(lods)), world_dt_min=float(deltas.min()), world_dt_max=float(deltas.max()),
            largest_temporal_changes=[dict(frame=i, mean_rgb_delta=v) for v,i in sorted(differences,reverse=True)[:5]]))
        sheet = Image.new('RGB', (960, 4*290), '#10141b')
        for j,(i,im) in enumerate(samples[:8]):
            im.thumbnail((480,270))
            x,y=(j%2)*480,(j//2)*290
            sheet.paste(im,(x,y+20))
            ImageDraw.Draw(sheet).text((x+5,y),f'{name} frame {i}', font=font,fill='white')
        sheet.save(out/(name+'_contact.jpg'), quality=94)
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    for mode in dict.fromkeys(p['mode'] for p in capture['plan']):
        video = out/(f'{mode}_{h}p.mp4')
        command = [ffmpeg,'-y','-loglevel','error','-f','rawvideo','-pixel_format','rgb24',
                   '-video_size',f'{w}x{h}','-framerate',str(fps),'-i','-',
                   '-an','-c:v','libx264','-preset','fast','-crf','16','-pix_fmt','yuv420p','-movflags','+faststart',str(video)]
        with (out/(mode+'_encode.log')).open('wb') as log:
            proc = subprocess.Popen(command,stdin=subprocess.PIPE,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                count=0
                for row in rows:
                    phase=phases[row['phase']]
                    if phase['mode'] != mode:
                        continue
                    im=Image.open(out/row['phase']/f"{row['frame']:05d}.png").convert('RGB')
                    draw=ImageDraw.Draw(im)
                    draw.rectangle((0,0,w//3 if mode=='sweep' else w,72*h//720),fill='#10141b')
                    title=f"Inori | {h}p | {phase['motion']} | frame {row['frame']:03d}"
                    if mode != 'sweep':
                        title += ' | 30 fps fixed-step PIE'
                    draw.text((20,10),title,font=font,fill='white')
                    label=('LEFT: LOD0    CENTER: LOD1    RIGHT: LOD2' if mode=='comparison' else
                           f"LOD {row['rendered_pose'][1]['lod']} | camera distance {row['distance_cm']:.0f} cm" if mode=='close' else
                           f"AUTO LOD {row['rendered_pose'][1]['lod']} | {row['distance_cm']:.0f} cm | 30 fps")
                    draw.text((20,40*h//720),label,font=font,fill='#74e9cd')
                    if any(capture['options'].get(k) for k in ['shadow_check','diagnostic','invalidation_check']):
                        draw.text((w//2,40*h//720),row['phase'],font=font,fill='#ffe3a3')
                    proc.stdin.write(im.tobytes())
                    count+=1
                proc.stdin.close()
                assert proc.wait(timeout=120)==0
            except BaseException:
                proc.kill()
                proc.wait()
                raise
        # Decode the encoded artifact, checking dimensions and frame count.
        reader=imageio_ffmpeg.read_frames(str(video))
        metadata=next(reader)
        assert tuple(metadata['size']) == (w,h)
        assert sum(1 for _ in reader)==count
        report['videos'].append(dict(path=str(video),frames=count,seconds=count/fps))
    (out/('review_'+args.mode+'.json' if args.mode else 'review.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2),flush=True)

if __name__=='__main__':
    main()

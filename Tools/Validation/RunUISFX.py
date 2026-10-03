"""Capture actual reward widgets through UE's UI submix, with audio enabled.

Checks state routing, double-confirm debounce, repeated opens and paused UI.
No microphone capture; exported PCM is the engine's rendered submix output.
"""
import array
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import sys
import uuid
import wave
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def read_wave(path):
    with wave.open(str(path),'rb') as stream:
        rate, channels, width = stream.getframerate(),stream.getnchannels(),stream.getsampwidth()
        assert width==2, (path,width)
        samples=array.array('h',stream.readframes(stream.getnframes()))
    if sys.byteorder!='little':
        samples.byteswap()
    mono=[sum(samples[i:i+channels])/channels/32768 for i in range(0,len(samples),channels)]
    return rate,mono,dict(channels=channels,sample_rate=rate,duration=len(mono)/rate,
                         peak=max(abs(v) for v in samples)/32768,
                         clipped_samples=sum(abs(v)>=32767 for v in samples))


def compare(actual, reference):
    # Locate by the strongest sample, then measure phase-aligned waveform fidelity.
    # Unity pitch + 48 kHz PCM should retain the signal apart from class/wave gain.
    shift=max(range(len(actual)),key=lambda i:abs(actual[i]))-max(range(len(reference)),key=lambda i:abs(reference[i]))
    best=(-1,0,0)
    for delta in range(shift-12,shift+13):
        pairs=[(actual[i+delta],reference[i]) for i in range(0,len(reference),4) if 0<=i+delta<len(actual)]
        xy=sum(a*b for a,b in pairs)
        xx=sum(a*a for a,b in pairs)
        yy=sum(b*b for a,b in pairs)
        if xx and yy:
            correlation=xy/math.sqrt(xx*yy)
            if correlation>best[0]:
                best=(correlation,xy/yy,delta)
    return dict(correlation=round(best[0],6),gain=round(best[1],6),offset_samples=best[2])


def main():
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)
    run_id='UI_SFX_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:8]
    out=ROOT/'Saved/QA'/run_id
    out.mkdir(parents=True)
    print('UI SFX evidence: '+str(out),flush=True)
    command=[engine/'Engine/Binaries/Win64/UnrealEditor.exe',ROOT/'UPlayground.uproject','/Game/Maps/RogueArena',
             '-game','-ExecCmds=t.MaxFPS 30,au.NeverDisableSubmixes 1,PGRewardProbe audio','-seconds=45','-RenderOffscreen','-windowed',
             '-ForceRes','-ResX=1280','-ResY=720','-nosplash','-unattended','-nop4','-AudioMixer','-DeterministicAudio',
             '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0,[Audio]:PlatformHeadroomDB=-3.0',
             '-PGTestProfile='+run_id,'-abslog='+str(out/'runtime.log')]
    code,timeout=run_process(command,ROOT,out/'stdout.log',150)
    log=read_text(out/'runtime.log') if (out/'runtime.log').exists() else ''
    problems=unexpected_errors(log,'UISFX')
    if code or timeout or FATAL.search(log):
        problems.append(f'Process failed: code={code} timeout={timeout}')
    if 'PGUISFX CAPTURES COMPLETE' not in log or 'PGRewardProbe FAIL' in log:
        problems.append('Audio fixture did not complete')
    manifest=json.loads(read_text(ROOT/'Tools/Art/UISFX/manifest.json'))
    refs=dict(zip(('open','confirm','victory'),manifest['sounds']))
    cases=[]
    for name in ('open','confirm','victory','pending','status','burst','paused'):
        case=dict(name=name,problems=[])
        path=out/'Audio'/(name+'.wav')
        try:
            rate,samples,stats=read_wave(path)
            case.update(stats)
            if stats['clipped_samples']:
                case['problems'].append('Output clipped')
            if name in ('pending','status'):
                if stats['peak']>1/32768:
                    case['problems'].append('Expected silent UI bus')
            else:
                ref=refs['open' if name=='burst' else 'victory' if name=='paused' else name]
                ref_rate,reference,_=read_wave(ROOT/'Tools/Art/UISFX'/ref['filename'])
                assert rate==ref_rate, (rate,ref_rate)
                case.update(compare(samples,reference))
                # UE BaseWindowsEngine.ini reserves -3 dB of platform headroom;
                # make it explicit above so the capture has a reproducible gain.
                expected_gain=ref['volume']*manifest['sound_class_volume']*10**(-3/20)
                if case['correlation']<.97:
                    case['problems'].append('Rendered waveform does not match expected sound')
                if abs(case['gain']-expected_gain)>.01:
                    case['problems'].append(f'Unexpected gain / stacked playback; expected {expected_gain}')
                if case['offset_samples']<0 or case['offset_samples']+len(reference)>len(samples):
                    case['problems'].append('Recording cut off the start or tail')
        except Exception as exc:
            case['problems'].append(repr(exc))
        case['status']='FAIL' if case['problems'] else 'PASS'
        cases.append(case)
        print(name+': '+case['status']+' '+str(case),flush=True)
    report=dict(status='FAIL' if problems or any(c['problems'] for c in cases) else 'PASS',problems=problems,cases=cases,
                command=[str(c) for c in command],subjective_listening=False,physical_input=False)
    (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return int(report['status']!='PASS')


if __name__=='__main__':
    raise SystemExit(main())

"""UE5.8 saved data, native regression, every contact and isolated build capture matrix."""
import argparse
from datetime import datetime,timezone
import json
import os
from pathlib import Path
import re
import struct
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from RunQA import run_process,read_text,FATAL,check_automation
from ReviewCombatVFX import first_cast,build_color,external_first_cast

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--render-only',action='store_true')
    parser.add_argument('--builds',action='store_true')
    parser.add_argument('--external',action='store_true',help='Require the external particle layer at every contact')
    parser.add_argument('--external-off',action='store_true',help='Render the authored layer alone for comparison')
    parser.add_argument('--build',type=int,choices=range(5),help='Only this build, for a focused visual iteration')
    args=parser.parse_args()
    if args.external and args.external_off: parser.error('External on/off checks are mutually exclusive')
    run=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out=ROOT/'Saved/QA'/('CombatVFX_'+run);out.mkdir(parents=True)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    common=[engine,ROOT/'UPlayground.uproject']
    flags=['-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-PGTestProfile=HackSlash_VFX_'+run]
    cases=[]
    script=['-nullrhi','-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(Path(__file__).with_name('ConfigureCombatVFX.py'))]
    if args.apply:cases.append(('apply',script,'PGCombatVFX APPLY PASS'))
    if not args.render_only:
        cases.append(('reload',script+['-PGCombatVFXValidate'],'PGCombatVFX VALIDATION PASS'))
        if args.external:
            cases.append(('external_reload',['-nullrhi','-EnablePlugins=PythonScriptPlugin','-run=pythonscript',
                '-script='+str(Path(__file__).with_name('ConfigureExternalVFX.py')),'-PGExternalVFXValidate'],'PGExternalVFX VALIDATE PASS'))
        cases.append(('automation',['-nullrhi','-ExecCmds=Automation RunTests PG.','-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(out/'Automation')],None))
    for build in ([args.build] if args.build is not None else [0,1,2,3,4] if args.builds else [0]):
        for p1 in (False,True):
            name=f'render_b{build}_p{int(p1)}'
            extra=['/Game/Maps/RogueArena','-game','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-PGHackSlashCapture','-PGSwingFXProbe','-PGCombatVFXFixedAim','-UseFixedTimeStep','-FPS=60','-ExecCmds=t.MaxFPS 60,pg.Skill.DebugCast 1,PGHackSlashProbe']
            if p1:extra.append('-PGHackSlashP1Probe')
            if args.external_off:
                extra=[v.replace('-ExecCmds=t.MaxFPS', '-ExecCmds=pg.Skill.ExternalVFX 0,t.MaxFPS') for v in extra]
            if build:extra+=['-PGSwingFXMiss','-PGCombatVFXBuild='+str(build)]
            cases.append((name,extra,'PGHackSlashProbe PASS skills='+('3 p1=1' if p1 else '5')))
    gates=[]
    print(out,flush=True)
    for name,extra,marker in cases:
        user=out/name/'User'
        command=common+extra+flags+['-UserDir='+str(user),'-abslog='+str(out/(name+'.log'))]
        code,timeout=run_process(command,ROOT,out/(name+'.stdout.log'),600)
        log=read_text(out/(name+'.log'))
        errors=[]
        if code or timeout or FATAL.search(log) or (marker and marker not in log):errors.append(f'code={code}, timeout={timeout}, marker={marker}')
        if re.search(r'Failed to compile Material|LogShaderCompilers: Error|LogPython: Error',log):errors.append('Shader/Python failure')
        if name=='automation':
            report=out/'Automation/index.json'
            if not report.exists():errors.append('Missing automation report')
            else: errors+=check_automation(json.loads(read_text(report)),['PG.HackSlash.CombatVFXBuildSnapshot'])[0]
        if name.startswith('render'):
            p1=name.endswith('p1');miss='_Miss' if not name.startswith('render_b0_') else ''
            for skill in ([110,113,114] if p1 else [100,101,102,111,112]):
                phases={110:5,111:4,112:6,113:5}.get(skill,1)
                for phase in range(phases):
                    suffix='' if phase==0 else '_Phase_'+str(phase)
                    path=user/'Saved/QA/HackSlashP0'/f'Skill_{skill}{miss}{suffix}.png'
                    if not path.exists() or struct.unpack('>II',path.read_bytes()[16:24])!=(1280,720):errors.append('Missing render '+str(path))
                    if f'PGCombatVFX Skill={skill} Phase={phase} Shape=' not in log:errors.append(f'Missing authored FX {skill}/{phase}')
                    if args.external and f'PGExternalVFX Skill={skill} Phase={phase} Shape=' not in log:errors.append(f'Missing external FX {skill}/{phase}')
            if not errors and name=='render_b0_p0':
                pixels=first_cast(user/'Saved/QA/HackSlashP0/Skill_100.png')
                (out/'first-cast-pixels.json').write_text(json.dumps(pixels,indent=2))
                if not pixels['pass_visible']:errors.append('Cold first cast is not visibly rendered')
                if args.external:
                    pixels=external_first_cast(user/'Saved/QA/HackSlashP0/Skill_100.png')
                    (out/'external-first-cast-pixels.json').write_text(json.dumps(pixels,indent=2))
                    if not pixels['pass_visible']:errors.append('Cold external mesh arc is not visibly rendered')
            if not errors and name.endswith('p0') and miss:
                pixels=build_color(user/'Saved/QA/HackSlashP0/Skill_100_Miss.png')
                (out/(name+'-pixels.json')).write_text(json.dumps(pixels,indent=2))
                if pixels['colored_pixels']<60:errors.append('Build color is not visibly rendered')
        gates.append(dict(name=name,status='FAIL' if errors else 'PASS',errors=errors,command=[str(c) for c in command]))
        print(name+': '+gates[-1]['status']+' '+str(errors),flush=True)
        if errors:break
    result=dict(status='FAIL' if any(g['errors'] for g in gates) else 'PASS',gates=gates,direct_play_verified=False,packaged_performance_verified=False)
    (out/'report.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(out/'report.json',flush=True)
    return int(result['status']!='PASS')
if __name__=='__main__':raise SystemExit(main())

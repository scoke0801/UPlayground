"""Run a read-only Bokusei hair-shadow diagnostic or saved-setting render check."""
import argparse
from datetime import datetime,timezone
import json
import os
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    modes=parser.add_mutually_exclusive_group()
    for flag in ['final','projection','path','geometry','culling','candidate','translucent-hq','layers','depth-bias','softness']:
        modes.add_argument('--'+flag,action='store_true')
    parser.add_argument('--temporal',action='store_true',help='Capture the fixed PIE viewport with settled TSR history')
    args=parser.parse_args()
    output=ROOT/'Saved/BokuseiHairShadowQuality'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    output.mkdir(parents=True)
    engine=Path('C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe')
    command=[engine,ROOT/'UPlayground.uproject','-unattended','-nosound','-nosplash','-nop4','-culture=en',
             '-DisablePlugins=RiderLink','-EnablePlugins=PythonScriptPlugin','-Multiprocess','-ddc=InstalledNoZenLocalFallback',
             '-ShaderWorkingDir='+str(ROOT/'Intermediate/BokuseiHairQualityShaders'),
             '-RenderOffscreen','-windowed','-ForceRes','-ResX=1600','-ResY=900',
             '-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeBokuseiHairShadowQuality.py'),
             '-abslog='+str(output/'engine.log')]
    if args.final:command.append('-PGHairQualityFinal')
    if args.projection:command.append('-PGHairQualityProjection')
    if args.path:command.append('-PGHairQualityPath')
    if args.geometry:command.append('-PGHairQualityGeometry')
    if args.culling:command.append('-PGHairQualityCulling')
    if args.candidate:command.append('-PGHairQualityCandidate')
    if args.layers:command.append('-PGHairQualityLayers')
    if args.depth_bias:command.append('-PGHairQualityDepthBias')
    if args.softness:command.append('-PGHairQualitySoftness')
    if args.temporal:command.append('-PGHairQualityTemporal')
    if args.translucent_hq:
        command += ['-PGHairQualityTranslucentHQ','-ini:Engine:[SystemSettings]:r.Shadow.Virtual.TranslucentQuality=1']
    (output/'command.json').write_text(json.dumps([str(c) for c in command],indent=2),encoding='utf-8')
    print(output,flush=True)
    with (output/'stdout.log').open('w',encoding='utf-8') as stream:
        process=subprocess.Popen([str(c) for c in command],cwd=ROOT,env=dict(os.environ,PG_HAIR_QUALITY_RUN=str(output)),
                    stdout=stream,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,
                    creationflags=subprocess.CREATE_NO_WINDOW)
        try:code=process.wait(timeout=280)
        except BaseException:
            if process.poll() is None:process.kill();process.wait(timeout=30)
            raise
    assert code==0,(code,output)
    report=json.loads((output/'quality.json').read_text(encoding='utf-8'))
    log=(output/'engine.log').read_text(encoding='utf-8-sig',errors='replace')
    assert report['status']=='PASS',report.get('error')
    assert not re.search(r'Fatal error:|LogShaderCompilers: Error|Failed to compile Material|LogPython: Error',log)
    print('Capture PASS',len(report['images']),flush=True)
    if args.candidate and args.temporal:
        subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',
                        str(ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison/MeasureHairShadowQuality.ps1'),
                        '-CaptureDirectory',str(output)],check=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)

if __name__=='__main__':main()

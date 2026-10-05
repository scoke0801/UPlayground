"""Run read-only FBX inspection and UE captures in isolated hidden processes."""
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/MoonlitUI/ModelReferences'
OUT.mkdir(parents=True, exist_ok=True)
commands = {
    'fbx': ['C:/Program Files/Blender Foundation/Blender 5.2/blender.exe', '--background', '--factory-startup',
            '--python', str(Path(__file__).with_name('InspectPortraitFBX.py'))],
    'capture': ['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe', str(ROOT/'UPlayground.uproject'),
        '-unattended', '-nosound', '-nosplash', '-nop4', '-culture=en', '-DisablePlugins=RiderLink',
        '-EnablePlugins=PythonScriptPlugin', '-ddc=InstalledNoZenLocalFallback', '-RenderOffscreen', '-windowed',
        '-ForceRes', '-ResX=1536', '-ResY=1536', '-ExecutePythonScript='+str(Path(__file__).with_name('CaptureModelReferences.py')),
        '-abslog='+str(OUT/'engine.log')]}
processes = {}
for name, cmd in commands.items():
    if '--capture-only' in sys.argv and name != 'capture': continue
    with (OUT/(name+'.log')).open('w') as log:
        processes[name] = subprocess.Popen(cmd, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=log,
            stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
report = {}
for name, p in processes.items():
    try: report[name] = p.wait(timeout=600)
    except subprocess.TimeoutExpired:
        p.kill()
        report[name] = 'TIMEOUT'
    print(name, report[name], flush=True)
(OUT/'processes.json').write_text(json.dumps(report, indent=2))
assert all(code == 0 for code in report.values()), report

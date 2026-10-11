"""U8 isolated process-to-process transactions and optional Development packaging.

Run RunQA.py --suite full first for automation, assets, six stages and 20 retries.
This probe uses synthetic transactions/ground drops, not physical input or balance.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, git_info, check_cycle, check_loot_cycle, check_retry, unexpected_errors


def check_phase(text, phase):
    rows = re.findall(r'PGInventoryU8 phase=(\w+) failures=(\d+) revision=(\d+)', text)
    return [] if len(rows)==1 and rows[0][0]==phase and rows[0][1]=='0' and 'PGInventoryU8 FAIL' not in text else ['Missing clean phase completion']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', action='store_true', help='Build a fresh Development package and test it')
    parser.add_argument('--executable', type=Path, help='Test an existing package; does not claim a fresh build')
    args = parser.parse_args()
    if args.package and args.executable:
        parser.error('Choose --package or --executable')
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine'
    out = ROOT/'Saved/QA'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:8]+'_inventory_cycle')
    out.mkdir(parents=True)
    report = dict(status='RUNNING', assisted=True, physical_input=False, source=git_info(ROOT),
                  content=git_info(ROOT/'Content'), gates=[], fresh_package=args.package,
                  not_covered=['Physical input, dynamic resolution, game feel and balance',
                               'Procedural dungeon checkpoint restoration',
                               'Abrupt process kill during a filesystem write'])

    def save():
        (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    def gate(name, command, seconds, validate):
        print(name+' started', flush=True)
        log = out/(name+'.log')
        command = [str(c) for c in command]
        entry = dict(name=name, command=command, log=str(log), status='RUNNING')
        report['gates'].append(entry); save()
        code, timeout = run_process(command, ROOT, out/(name+'.stdout.log'), seconds)
        text = read_text(log) if log.exists() else ''
        errors = validate(text) + unexpected_errors(text, name)
        if code or timeout or not text or FATAL.search(text):
            errors.append(f'Process failure: exit={code} timeout={timeout} log={bool(text)}')
        entry.update(status='FAIL' if errors else 'PASS', exit_code=code, timeout=timeout, errors=errors)
        save(); print(name+' '+entry['status']+' '+str(errors), flush=True)
        return not errors

    print(out, flush=True); save()
    exe = args.executable.resolve() if args.executable else None
    if args.package:
        dotnet = list((engine/'Binaries/ThirdParty/DotNet').glob('*/win-x64/dotnet.exe'))
        if len(dotnet) != 1:
            raise RuntimeError('Expected one engine-bundled dotnet')
        command = [dotnet[0], engine/'Binaries/DotNET/AutomationTool/AutomationTool.dll',
                   '-ScriptsForProject='+str(ROOT/'UPlayground.uproject'), 'BuildCookRun',
                   '-project='+str(ROOT/'UPlayground.uproject'), '-noP4', '-unattended', '-utf8output',
                   '-platform=Win64', '-clientconfig=Development', '-build', '-cook', '-stage', '-pak', '-iostore',
                   '-map=/Game/Maps/RogueArena+/Game/Maps/L_PG_ProceduralDungeon', '-stagingdirectory='+str(out/'Package'),
                   '-ddc=InstalledNoZenLocalFallback']
        code, timeout = run_process(command, ROOT, out/'package.log', 7200)
        candidates = list((out/'Package').glob('**/UPlayground/Binaries/Win64/UPlayground.exe'))
        text = read_text(out/'package.log')
        ok = not code and not timeout and 'BUILD SUCCESSFUL' in text and len(candidates)==1 and not FATAL.search(text)
        report['gates'].append(dict(name='package', status='PASS' if ok else 'FAIL', exit_code=code,
                                   timeout=timeout, command=[str(c) for c in command], log=str(out/'package.log')))
        save()
        if ok: exe = candidates[0]
        else:
            report['status']='FAIL'; save(); return 1
    if exe:
        report['executable']=str(exe)
        report['executable_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
        if args.package:
            report['play_map']='/Game/Maps/L_PG_ProceduralDungeon'
    base = [exe] if exe else [engine/'Binaries/Win64/UnrealEditor-Cmd.exe', ROOT/'UPlayground.uproject']
    profile = 'UI_U8_'+uuid.uuid4().hex[:20]
    common = ['/Game/Maps/RogueArena', '-game', '-nullrhi', '-nosound', '-unattended', '-nop4',
              '-culture=ko', '-ddc=InstalledNoZenLocalFallback']
    for phase in ('u8seed', 'u8resume', 'u8result'):
        command = base+common+['-PGTestProfile='+profile, '-ExecCmds=t.MaxFPS 60,PGInventoryProbe '+phase,
                               '-abslog='+str(out/(phase+'.log'))]
        def validate(text, phase=phase):
            return check_phase(text, phase)
        if not gate(phase, command, 180, validate): break
    if exe:
        if args.package:
            command = [exe, '-game', '-nullrhi', '-nosound', '-unattended', '-nop4',
                       '-culture=ko', '-ddc=InstalledNoZenLocalFallback', '-seconds=40',
                       '-PGTestProfile=U10_Default_'+uuid.uuid4().hex[:16],
                       '-UserDir='+str(out/'DefaultMapUser'), '-abslog='+str(out/'default_map.log')]
            def check_default_map(text):
                return [] if 'Bringing World /Game/Maps/L_PG_ProceduralDungeon' in text and 'PGDungeon objective waiting=1' in text else ['Default PCG map did not reach traversal']
            gate('default_map', command, 240, check_default_map)
        for name, extra, validate in (
            ('cycle', ['-PGRunSeed=173001', '-ExecCmds=t.MaxFPS 60,PGCombatCycleSmoke', '-seconds=240',
                       '-TestExit=PGCombatCycle COMPLETE+PGCombatCycle FAILED+PGCombatCycle TIMEOUT'],
             lambda text: check_cycle(text)+check_loot_cycle(text)),
            ('retry', ['-PGRetryProbe=20'], check_retry)):
            command = base+common+['-PGRogueAutoStart', '-PGTestProfile=U8_'+uuid.uuid4().hex[:20],
                                   '-abslog='+str(out/(name+'.log'))]+extra
            gate(name, command, 600, validate)
    report['status']='PASS' if all(g['status']=='PASS' for g in report['gates']) else 'FAIL'
    save(); print(report['status']+': '+str(out/'report.json'), flush=True)
    return report['status']!='PASS'


if __name__ == '__main__':
    raise SystemExit(main())

"""Isolated inventory fixtures, Slate key routing checks, and real-game captures.

Uses a fresh UI_* test profile for every process; never touches the player profile.
Run after an editor build with Unreal's bundled Python.
"""
import argparse
from datetime import datetime, timezone
import json
import os
import re
from pathlib import Path
import shutil
import struct
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors
from PlayableCharacterCatalog import PLAYER_IDS


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, help='Render and validate a cooked Development executable')
    parser.add_argument('--cases', default='open720,open1080,wide,full,empty,build,build1080,buildwide,character,character1080,characterwide,failure,interaction,states,design,hud720,hud1080')
    portraits = PLAYER_IDS
    parser.add_argument('--portrait-previews', nargs='*', choices=portraits, default=[],
                        help='Also capture each requested portrait through its real gallery button at 1080p.')
    args = parser.parse_args()
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / ('UE_' + version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT / 'Saved/QA' / (run_id + '_inventory')
    out.mkdir(parents=True)
    cases = dict(open720=(1280, 720, 'open'), open1080=(1920, 1080, 'open'), wide=(2560, 1080, 'open'),
                 hud720=(1280,720,'open'), hud1080=(1920,1080,'open'),
                 full=(1280, 720, 'full'), empty=(1280, 720, 'empty'), build=(1280, 720, 'build'),
                 build1080=(1920,1080,'build'), buildwide=(2560,1080,'build'), character=(1280,720,'character'), character1080=(1920,1080,'character'), characterwide=(2560,1080,'character'), failure=(1280, 720, 'failure'), interaction=(1280, 720, 'open'), states=(1280, 720, 'open'), design=(1280,720,'build'))
    cases.update(stats720=(1280,720,'stats'), stats1080=(1920,1080,'stats'), statswide=(2560,1080,'stats'))
    cases.update(level720=(1280,720,'stats'), level1080=(1920,1080,'stats'), levelmax=(1280,720,'stats'))
    cases.update(growth720=(1280,720,'stats'), growth1080=(1920,1080,'stats'))
    cases.update(notice720=(1280,720,'stats'))
    cases.update(salvage720=(1280,720,'open'), salvage1080=(1920,1080,'open'), salvagecombat=(1280,720,'open'))
    cases.update(imprint720=(1280,720,'open'), imprint1080=(1920,1080,'open'), imprintcombat=(1280,720,'open'))
    cases.update(craft720=(1280,720,'open'), craft1080=(1920,1080,'open'), craftcombat=(1280,720,'open'))
    cases.update(crafttemper720=(1280,720,'open'), crafttemper1080=(1920,1080,'open'))
    cases.update(hudgrowth=(1280,720,'open'))
    cases.update(statsdetail720=(1280,720,'stats'), statsdetail1080=(1920,1080,'stats'), optionsdetail1080=(1920,1080,'open'))
    cases.update(options720=(1280,720,'open'), options1080=(1920,1080,'open'))
    cases.update(expansion720=(1280,720,'open'), expansion1080=(1920,1080,'open'))
    cases.update({f'portrait_{name}': (1920,1080,'character') for name in args.portrait_previews})
    requested = [name for name in args.cases.split(',') if name] + [f'portrait_{name}' for name in args.portrait_previews]
    if not requested:
        parser.error('At least one case or portrait preview is required')
    unknown = sorted(set(requested) - cases.keys())
    if unknown:
        parser.error('Unknown cases: ' + ', '.join(unknown))
    executable = args.executable.resolve() if args.executable else None
    if executable and not executable.is_file():
        parser.error('Packaged executable does not exist')
    report = dict(status='RUNNING', assisted=True, physical_input=False, executable=str(executable) if executable else None, requested_cases=requested, cases=[])
    (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('Inventory evidence: ' + str(out), flush=True)
    for name in requested:
        width, height, mode = cases[name]
        user_dir = out / 'Users' / name
        screenshots = (user_dir if executable else ROOT) / 'Saved/Screenshots'
        before = {p: p.stat().st_mtime_ns for p in screenshots.rglob('*.png')}
        commands = 't.MaxFPS 30,PGInventoryProbe ' + mode
        if name == 'hudgrowth':
            commands = 't.MaxFPS 30,PGExperience 650,PGInventoryProbe open,PGInventoryProbe stats'
        if name.startswith('level'):
            amount = 2147483647 if name == 'levelmax' else 650
            commands = f't.MaxFPS 30,PGExperience {amount},PGInventoryProbe ' + mode
        if name == 'interaction':
            commands += ',PGInventoryProbe verify'
        if name.startswith('growth'):
            commands += ',PGInventoryProbe verifyGrowth'
        if name.startswith('notice'):
            commands += ',PGInventoryProbe verifyGrowthNotice'
        if name.startswith('imprint'):
            commands += ',PGInventoryProbe verifyImprint'
        if name.startswith('craft'):
            commands += ',PGInventoryProbe verifyCraft'
        if name.startswith('salvage'):
            commands += ',PGInventoryProbe verifySalvage'
        if name == 'design':
            commands += ',PGInventoryProbe verifyDesign'
        if name == 'states':
            commands += ',PGInventoryProbe verifyStates'
        if name.startswith('hud'):
            commands += ',PGInventoryProbe close'
        profile = 'UI_' + uuid.uuid4().hex[:20]
        command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject', '/Game/Maps/RogueArena',
                   '-game', '-ExecCmds=' + commands, '-seconds=90', '-testexit=Tracing Screenshot', '-RenderOffscreen', '-windowed', '-ForceRes',
                   f'-ResX={width}', f'-ResY={height}', '-nosplash', '-nosound', '-unattended', '-nop4',
                   '-culture=ko', '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback',
                   '-PGTestProfile=' + profile, f'-abslog={out / (name + ".log")}']
        if executable:
            command = [executable] + command[2:] + ['-UserDir=' + str(user_dir)]
            # Cooked builds may omit screenshot tracing, so its TestExit marker never arrives.
            command[command.index('-seconds=90')] = '-seconds=20'
        if mode == 'stats':
            command.append('-PGValidateStats')
        if name == 'imprintcombat':
            command.append('-PGImprintCombat')
        if name == 'craftcombat':
            command.append('-PGCraftCombat')
        if name.startswith('crafttemper'):
            command.append('-PGTemperPreview')
        if name == 'salvagecombat':
            command.append('-PGSalvageCombat')
        if 'detail' in name:
            command.append('-PGCaptureDetails')
        if name.startswith('options'):
            command.append('-PGValidateOptions')
        if name.startswith('expansion'):
            command.append('-PGEquipmentExpansion')
        if mode == 'character':
            command.append('-PGValidatePortraits')
            if name.startswith('portrait_'):
                command.append('-PGPortraitPreview=' + name.removeprefix('portrait_'))
        code, timeout = run_process(command, ROOT, out / (name + '.stdout.log'), 180)
        path = out / (name + '.log')
        log = read_text(path) if path.is_file() else ''
        problems = unexpected_errors(log, name)
        if name.startswith('expansion') and not re.search(r'PGInventoryProbe EXPANSION failures=0 slots=(9|10)', log):
            problems.append('Expanded slot equip or actual GAS stat application failed')
        if name.startswith('imprint') and 'PGInventoryProbe IMPRINT failures=0' not in log:
            problems.append('Imprint transaction checks did not pass')
        if name.startswith('craft') and 'PGInventoryProbe CRAFT failures=0' not in log:
            problems.append('Craft transaction checks did not pass')
        if name == 'craftcombat' and 'PGInventoryProbe CRAFT COMBAT closed=1' not in log:
            problems.append('Combat did not close the crafting draft')
        if name.startswith('growth') and 'PGInventoryProbe GROWTH failures=0' not in log:
            problems.append('Growth interaction checks did not pass')
        if name.startswith('notice') and 'PGInventoryProbe NOTICE failures=0' not in log:
            problems.append('Growth notification checks did not pass')
        if name.startswith('salvage') and 'PGInventoryProbe SALVAGE failures=0' not in log:
            problems.append('Salvage transaction checks did not pass')
        if name.startswith('level'):
            expected = 'level=20 xp=0 earned=57 unspent=57' if name == 'levelmax' else 'level=4 xp=50 earned=9 unspent=9'
            if 'PGExperience saved=1 ' + expected not in log:
                problems.append('Level award did not persist the expected state')
        if code or timeout or FATAL.search(log):
            problems.append(f'Process failed: code={code} timeout={timeout}')
        if 'PGInventoryProbe OPEN mode=' + mode not in log:
            problems.append('Inventory fixture did not open')
        if mode == 'stats' and 'PGInventoryProbe STATS values=7 failures=0' not in log:
            problems.append('Displayed stat values do not match the player stat component')
        if 'detail' in name and not re.search(r'PGInventoryProbe DETAILS scrolled=[1-9]\d*', log):
            problems.append('Detail scroll capture did not run')
        if name.startswith('options') and 'PGInventoryProbe OPTIONS matched=2 failures=0' not in log:
            problems.append('Generated base/affix values and stored ranges are not displayed correctly')
        if name == 'imprintcombat' and 'PGInventoryProbe IMPRINT COMBAT closed=1' not in log:
            problems.append('Combat did not close the imprint draft')
        if name == 'salvagecombat' and 'PGInventoryProbe SALVAGE COMBAT closed=1' not in log:
            problems.append('Combat transition did not close the pending inventory')
        if not name.startswith('hud') and name not in ('salvagecombat', 'imprintcombat', 'craftcombat') and not re.search(r'PGInventoryProbe OVERLAY .* failures=0', log):
            problems.append('Overlay geometry or input mapping checks did not pass')
        if 'PGInventoryProbe CAPTURE ready' not in log:
            problems.append('No capture after asset compilation completed')
        if mode == 'character':
            preview = name.removeprefix('portrait_') if name.startswith('portrait_') else portraits[0]
            if f'PGInventoryProbe PORTRAITS images={len(portraits)+1} failures=0 preview={preview}' not in log:
                problems.append('Portrait aspect ratio, gallery coverage or unsaved preview check failed')
        if name == 'interaction' and 'PGInventoryProbe CHECK failures=0' not in log:
            problems.append('Interaction checks did not pass')
        if name == 'states' and 'PGInventoryProbe STATES failures=0' not in log:
            problems.append('Modal state checks did not pass')
        if name == 'design' and 'PGInventoryProbe DESIGN failures=0' not in log:
            problems.append('Design interaction checks did not pass')
        captures = []
        for image in sorted(screenshots.rglob('PGInventory*.png')):
            if before.get(image) != image.stat().st_mtime_ns:
                target = out / (name + '_' + image.name)
                shutil.copy2(image, target)
                if struct.unpack('>II', target.read_bytes()[16:24]) != (width, height):
                    problems.append('Wrong screenshot dimensions: ' + target.name)
                captures.append(target.name)
        if not captures:
            problems.append('No current-run screenshot')
        entry = dict(name=name, status='FAIL' if problems else 'PASS', problems=problems, screenshots=captures,
                     command=[str(a) for a in command])
        report['cases'].append(entry)
        report['status'] = 'FAIL' if any(c['problems'] for c in report['cases']) else 'RUNNING'
        (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(name + ': ' + entry['status'] + ' ' + str(problems), flush=True)
    report['status'] = 'FAIL' if any(c['problems'] for c in report['cases']) else 'PASS'
    (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())

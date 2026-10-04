"""Export per-capture CPU statistics from RunToonPerformance --trace.

Trace regions exclude map loading, warmup, phase setup and screenshots.
Inclusive nested scopes and parallel threads must not be added as frame time.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
THREADS = {'game': 'GameThread', 'render': 'Render*', 'rhi': 'RHIThread',
           'rhi_interrupt': 'RHIInterruptThread',
           'rhi_submission': 'RHISubmissionThread',
           'workers': 'Foreground*,Background*,TaskGraph*,Worker*'}


def longest_frames(events, start, end):
    # Insights event export includes events overlapping the region boundaries.
    # In particular the last frame may contain CSV shutdown/phase setup after
    # RegionEnd. Do not mislabel that work as a measured runtime hitch.
    frames = sorted((r for r in events if r['TimerName'] == 'FEngineLoop::Tick'
                     and start <= float(r['StartTime']) and float(r['EndTime']) <= end),
                    key=lambda r: float(r['Duration']), reverse=True)
    slowest = []
    for frame in frames[:5]:
        frame_start, frame_end = float(frame['StartTime']), float(frame['EndTime'])
        children = [r for r in events if r['TimerName'] != 'FEngineLoop::Tick'
                    and frame_start <= float(r['StartTime']) and float(r['EndTime']) <= frame_end]
        slowest.append(dict(frame=frame, scopes=sorted(children, key=lambda r: float(r['Duration']), reverse=True)))
    return slowest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    out = args.directory.resolve()
    report = json.loads((out/'report.json').read_text(encoding='utf-8'))
    runtime = json.loads((out/'runtime.json').read_text(encoding='utf-8'))
    if runtime['status'] != 'PASS' or not report.get('options', {}).get('trace'):
        parser.error('Requires a completed --trace fixture')
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/('UE_'+version)
    commands = [f'TimingInsights.ExportThreads "{(out/"cpu_threads.csv").as_posix()}"']
    for group, pattern in THREADS.items():
        target = (out/('cpu_'+group+'_{region}.csv')).as_posix()
        commands.append(f'TimingInsights.ExportTimerStatistics "{target}" -threads={pattern} -region=PGToon_*')
    events = (out/'cpu_events_{region}.csv').as_posix()
    commands.append(f'TimingInsights.ExportTimingEvents "{events}" -threads=GameThread '
                    '-timers=FEngineLoop::Tick,FDirectoryWatcherWindows::Tick,Slate::Tick*,UMassEntityEditorSubsystem::Tick,Sync_RenderingThread,Tick_Engine,UWorld_Tick,TG_*,ProcessUntilTasksComplete,WaitForTasks '
                    '-columns=ThreadName,TimerName,StartTime,EndTime,Duration,Depth -region=PGToon_*')
    command_file = out/'export_cpu.txt'
    command_file.write_text('\n'.join(commands)+'\n', encoding='utf-8')
    command = [str(engine/'Engine/Binaries/Win64/UnrealInsights.exe'),
               '-OpenTraceFile='+str(out/'cpu.utrace'), '-AutoQuit', '-NoUI',
               '-ExecOnAnalysisCompleteCmd=@='+str(command_file), '-abslog='+str(out/'insights.log')]
    with (out/'insights_stdout.log').open('w', encoding='utf-8') as stream:
        subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                       timeout=180, check=True, creationflags=subprocess.CREATE_NO_WINDOW)
    export_log = (out/'insights.log').read_text(encoding='utf-8-sig', errors='replace')
    regions = {name: (float(start), float(end)) for name, start, end in re.findall(
        r"for region '(PGToon_[^']+)' \[([\d.]+) \.\. ([\d.]+)\]", export_log)}
    summary = dict(timing_status=report['status'], regions=regions, phases={}, longest_frames={},
                   limits=['Trace overhead is included; use untraced CSV for GPU cost attribution',
                           'Exclusive timer time includes waits; it is not CPU utilization',
                           'Inclusive scopes overlap; worker-thread totals are parallel CPU time'])
    for phase in runtime['phases']:
        groups = {}
        for group in THREADS:
            path = out/('cpu_'+group+'_PGToon_'+phase['name']+'.csv')
            rows = list(csv.DictReader(path.open(encoding='utf-8-sig')))
            active = [r for r in rows if float(r['Count']) > 0]
            if group in ('game', 'render') and not active:
                raise ValueError('Missing CPU events: '+str(path))
            groups[group] = sorted(active, key=lambda r: float(r['Excl']), reverse=True)[:40]
        summary['phases'][phase['name']] = groups
        events_path = out/('cpu_events_PGToon_'+phase['name']+'.csv')
        events = list(csv.DictReader(events_path.open(encoding='utf-8-sig')))
        slowest = longest_frames(events, *regions['PGToon_'+phase['name']])
        if not slowest:
            raise ValueError('Missing frame events: '+str(events_path))
        summary['longest_frames'][phase['name']] = slowest
    (out/'cpu_summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    for phase, groups in summary['phases'].items():
        for group, rows in groups.items():
            print(phase, group, [(r['Name'], r['Excl'], r['Incl']) for r in rows[:8]])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

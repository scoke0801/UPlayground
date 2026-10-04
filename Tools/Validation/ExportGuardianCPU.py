"""Export a bounded GameThread CPU summary from a guardian soak Insights trace."""
import argparse
import csv
import json
import os
from pathlib import Path
import subprocess
from RunQA import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--start', type=float, default=0)
    parser.add_argument('--end', type=float, default=1e9)
    parser.add_argument('--threads', default='GameThread')
    args = parser.parse_args()
    out = args.directory.resolve()
    version = json.loads((ROOT/'UPlayground.uproject').read_text(encoding='utf-8-sig'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/f'UE_{version}'
    output_name = 'cpu_game.csv' if args.threads == 'GameThread' else 'cpu_workers.csv'
    command_file = out/'export_cpu.txt'
    command_file.write_text(
        f'TimingInsights.ExportTimerStatistics "{(out / output_name).as_posix()}" -threads={args.threads} '
        f'-startTime={args.start} -endTime={args.end}\n', encoding='utf-8')
    command = [str(engine/'Engine/Binaries/Win64/UnrealInsights.exe'),
        f'-OpenTraceFile={out/"cpu.utrace"}', '-AutoQuit', '-NoUI',
        f'-ExecOnAnalysisCompleteCmd=@={command_file}', f'-abslog={out/"insights.log"}']
    result = subprocess.run(command, cwd=ROOT, timeout=180, capture_output=True)
    if result.returncode or not (out/output_name).exists():
        raise RuntimeError(f'Insights export failed: {result.returncode}; see {out / "insights.log"}')
    rows = list(csv.DictReader((out/output_name).open(encoding='utf-8-sig')))
    print('Columns:', ', '.join(rows[0]) if rows else 'NO EVENTS')
    for row in sorted(rows, key=lambda r: float(r.get('Excl', 0)), reverse=True)[:30]:
        print(row['Name'], 'count='+row['Count'], 'exclusive_seconds='+row['Excl'], 'inclusive_seconds='+row['Incl'], 'max_seconds='+row['I.Max'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

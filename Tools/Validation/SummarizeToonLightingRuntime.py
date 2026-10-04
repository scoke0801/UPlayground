"""Summarize the latest toon fixture CSVs; requires actual 1280x720 PIE output."""
import csv
import json
import math
import os
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
runtime = json.loads((ROOT/'Saved/ToonTest/lighting_runtime.json').read_text())
assert runtime['status'] == 'PASS', runtime.get('error', 'Runtime incomplete')
assert runtime['viewport_size'] == [1280, 720], 'Reject panel-sized measurements'
assert len(runtime['phases']) == 6
report = {'status': 'CHARACTERIZED', 'runtime': runtime['run'], 'viewport_size': runtime['viewport_size'],
          'settings': runtime['render_settings'], 'phases': [], 'warnings': [],
          'limits': ['Editor PIE, isolated animated Inori fixture without combat/AI; not packaged performance',
                     '12-second samples after 8-second warmup; not a long soak or memory-leak proof',
                     'Outline comparison disables both CustomDepth and the outline blendable; timing noise remains']}
try:
    report['gpu'] = subprocess.run(['nvidia-smi', '--query-gpu=name,driver_version', '--format=csv,noheader'],
        capture_output=True, text=True, timeout=10).stdout.strip()
except (OSError, subprocess.TimeoutExpired):
    report['gpu'] = 'unavailable'
report['cpu'] = os.environ.get('PROCESSOR_IDENTIFIER', 'unavailable')
csv.field_size_limit(10_000_000)
for phase in runtime['phases']:
    path = Path(phase['csv'])
    values = {}
    frames = 0
    with path.open(encoding='utf-8-sig') as stream:
        for row in csv.DictReader(stream):
            try:
                if not math.isfinite(float(row['FrameTime'])) or float(row['FrameTime']) <= 0:
                    continue
            except (ValueError, TypeError, KeyError):
                continue
            frames += 1
            for key, raw in row.items():
                if key not in ['FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime', 'GPUMem/LocalUsedMB'] and not str(key).startswith('GPU/'):
                    continue
                try:
                    value = float(raw)
                    if math.isfinite(value):
                        values.setdefault(key, []).append(value)
                except (ValueError, TypeError):
                    pass
    assert frames >= 120, (path, frames)
    metrics = {key: {'mean': round(statistics.mean(v), 3), 'p95': round(sorted(v)[math.ceil(len(v)*.95)-1], 3)} for key, v in values.items()}
    assert metrics['GPUTime']['mean'] > 0
    report['phases'].append({**phase, 'frames': frames, 'metrics': metrics})
    if metrics['FrameTime']['p95'] > 16.7:
        report['warnings'].append(phase['name']+': frame p95 exceeds 16.7 ms')
by_name = {p['name']: p for p in report['phases']}
on = statistics.mean(by_name[n]['metrics']['GPUTime']['mean'] for n in ['fifty_outline', 'fifty_outline_repeat'])
off = by_name['fifty_no_outline']['metrics']['GPUTime']['mean']
first_gpu = by_name['fifty_outline']['metrics']['GPUTime']['mean']
repeat_gpu = by_name['fifty_outline_repeat']['metrics']['GPUTime']['mean']
ratio = max(first_gpu, repeat_gpu)/min(first_gpu, repeat_gpu)
report['repeatability_gpu_ratio'] = round(ratio, 3)
if ratio > 1.25:
    report['status'] = 'UNSTABLE_PERFORMANCE_SAMPLE'
    report['observed_outline_bundle_gpu_delta_ms'] = None
    report['warnings'].append('Repeated identical 50-character conditions differ by more than 25%; do not attribute an outline cost or certify a frame budget')
else:
    report['observed_outline_bundle_gpu_delta_ms'] = round(on-off, 3)
text = json.dumps(report, indent=2)
(ROOT/'Saved/ToonTest/lighting_performance.json').write_text(text, encoding='utf-8')
(Path(runtime['run'])/'performance.json').write_text(text, encoding='utf-8')
print(json.dumps({k: v for k, v in report.items() if k != 'phases'}, indent=2))
for phase in report['phases']:
    print(phase['name'], phase['frames'], {k: phase['metrics'][k] for k in ['FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime']})

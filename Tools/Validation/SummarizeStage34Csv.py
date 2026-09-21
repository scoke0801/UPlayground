"""Summarize actual frame rows, excluding CSV footer metadata and startup frames."""
import csv
import json
import math
import statistics
import sys
from pathlib import Path

csv.field_size_limit(10_000_000)
path = Path(sys.argv[1])
with path.open(encoding="utf-8-sig") as stream:
    rows = []
    for row in csv.DictReader(stream):
        try:
            frame = float(row["FrameTime"])
        except (ValueError, TypeError, KeyError):
            continue
        if math.isfinite(frame):
            rows.append(row)
rows = rows[120:]
keys = ["FrameTime", "GameThreadTime", "RenderThreadTime", "GPUTime", "Slate/GameThread/DrawPrePass", "Exclusive/GameThread/UI", "Exclusive/GameThread/BehaviorTreeTick", "Exclusive/GameThread/ProjectileMovement", "Memory/PhysicalUsedMB"]
summary = {"source": str(path), "warmup_frames_excluded": 120, "frames": len(rows), "metrics_ms": {}}
for key in keys:
    values = []
    for row in rows:
        try:
            values.append(float(row[key]))
        except (ValueError, KeyError, TypeError):
            pass
    if values:
        values.sort()
        summary["metrics_ms"][key] = {"mean": round(statistics.mean(values), 3), "p95": round(values[int(.95 * (len(values)-1))], 3), "p99": round(values[int(.99 * (len(values)-1))], 3)}
path.with_suffix(".summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
print(json.dumps(summary, indent=2))

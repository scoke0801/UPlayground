"""Compare rendered standalone/PIE navigation with health-assisted probes.

Does not inject player input and cannot approve gameplay feel or combat balance.
Run only after RunQA has finished; screenshots/logs remain in Saved/QA.
"""
from datetime import datetime, timezone
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import uuid

from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("both", "standalone", "pie"), default="both")
    parser.add_argument("--culture", help="Override this process only, e.g. en to diagnose localized engine smoke tests")
    options = parser.parse_args()
    engine_version = json.loads(read_text(ROOT / "UPlayground.uproject"))["EngineAssociation"]
    engine = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Epic Games" / f"UE_{engine_version}"
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:8]
    output = ROOT / "Saved/QA" / (run_id + "_navigation")
    output.mkdir(parents=True)
    print(f"Presentation evidence: {output}", flush=True)
    report = dict(assisted=True, direct_input=False, culture=options.culture or "system", gates=[])
    for mode in (("standalone", "pie") if options.mode == "both" else (options.mode,)):
        screenshots = ROOT / "Saved/Screenshots"
        before = {p: p.stat().st_mtime_ns for p in screenshots.rglob("*.png")}
        log_path = output / f"{mode}.log"
        args = [engine / "Engine/Binaries/Win64/UnrealEditor.exe", ROOT / "UPlayground.uproject"]
        if mode == "standalone":
            args += ["/Game/Maps/RogueArena", "-game", "-PGRogueAutoStart", "-PGControlsExit", "-ExecCmds=t.MaxFPS 60,PGCombatControlsProbe"]
        else:
            args += ["-EnablePlugins=PythonScriptPlugin", f"-ExecutePythonScript={ROOT / 'Tools/Validation/ProbeRogueNavigationPIE.py'}"]
        args += ["-RenderOffscreen", "-windowed", "-ResX=1280", "-ResY=720", "-nosplash", "-unattended", "-nosound",
                 "-nop4", "-UTF8Output", "-LogCmds=LogAutomationTest Log", "-PGCaptureProbe", "-PGRunSeed=173001",
                 f"-PGTestProfile=Nav_{run_id}_{mode}", f"-abslog={log_path}"]
        if options.culture:
            args.append(f"-culture={options.culture}")
        code, timeout = run_process(args, ROOT, output / f"{mode}.stdout.log", 300)
        log = read_text(log_path) if log_path.is_file() else ""
        errors = unexpected_errors(log, mode)
        if code or timeout or FATAL.search(log):
            errors.append(f"Process failed: code={code}, timeout={timeout}")
        if not all(f"PGControls t={t}" in log for t in (2, 8, 16)):
            errors.append("Missing timed observations")
        if len(re.findall(r"PGControls nav=1 building=0 playerProjection=1", log)) != 3:
            errors.append("Navigation was not ready at every observation")
        moving = re.findall(r"PGControls enemy=.+ speed=([\d.]+).+ path=1 partial=0", log)
        if not any(float(speed) > 0 for speed in moving):
            errors.append("No moving enemy with a complete player path was observed")
        captures = []
        for path in screenshots.rglob("*.png"):
            if before.get(path) != path.stat().st_mtime_ns:
                target = output / (mode + "_" + path.name)
                shutil.copy2(path, target)
                captures.append(target.name)
        if not captures:
            errors.append("Rendered screenshot missing")
        gate = dict(mode=mode, status="FAIL" if errors else "PASS", problems=errors,
                    command=[str(a) for a in args], screenshots=captures)
        report["gates"].append(gate)
        (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"{mode}: {gate['status']} ({len(errors)} errors; see report.json)", flush=True)
    return int(any(g["status"] != "PASS" for g in report["gates"]))


if __name__ == "__main__":
    raise SystemExit(main())

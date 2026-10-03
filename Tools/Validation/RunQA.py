"""Run the existing Unreal QA gates, preserving evidence in a unique Saved directory.

Uses only the standard library; RunQA.ps1 locates Unreal's bundled Python on Windows.
Headless progression uses assisted kills/health and does NOT validate combat balance.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
FATAL = re.compile(r"Fatal error:|Assertion failed:|Unhandled Exception:|Ensure condition failed:", re.I)
EXPECTED_DEATH = re.compile(r"LogTemp: Error: Stage 1 failed: Player defeated\.$")


def read_text(path):
    return Path(path).read_text(encoding="utf-8-sig", errors="replace")


def check_automation(report, expected):
    tests = report.get("tests", [])
    by_name = {test["fullTestPath"]: test for test in tests}
    problems = []
    missing = sorted(set(expected) - by_name.keys())
    if missing:
        problems.append("Tests missing from report: " + ", ".join(missing))
    if not tests:
        problems.append("No automation tests ran")
    for key in ("failed", "notRun", "inProcess"):
        if report.get(key, -1) != 0:
            problems.append(f"Automation {key}={report.get(key, 'missing')}")
    for name, test in by_name.items():
        if test.get("state") != "Success" or test.get("errors", 0):
            problems.append(f"{name}: {test.get('state')} errors={test.get('errors', 0)}")
    warnings = [f"{name}: {entry['event']['message']}"
                for name, test in by_name.items() for entry in test.get("entries", [])
                if entry.get("event", {}).get("type") == "Warning"]
    return problems, warnings


def check_cycle(log):
    # Check the complete current MVP sequence, not just a final success string.
    # Unreal echoes its command line, which contains the success/failure exit phrases.
    log = "\n".join(line for line in log.splitlines() if "-testexit=" not in line.lower())
    waves = [(int(stage), int(wave), int(total)) for stage, wave, total in
             re.findall(r"PGWave started stage=(\d+) wave=(\d+)/(\d+)", log)]
    expected = [(stage, wave, 3) for stage in range(1, 6) for wave in range(1, 4)] + [(6, 1, 1)]
    selected = Counter(int(stage) for stage in re.findall(r"PGCombatCycle select stage=(\d+)", log))
    applied = Counter(int(stage) for stage in re.findall(r"PGReward applied stage=(\d+)", log))
    expected_rewards = Counter({1: 1, 2: 2, 3: 1, 4: 2, 5: 1})
    problems = []
    if waves != expected:
        problems.append(f"Wave sequence mismatch: {waves!r}")
    # PGReward applied is emitted only after the LAST choice of each stage.
    # The first choices in stages 2/4 commit before the next card is offered.
    expected_final_commits = Counter({stage: 1 for stage in range(1, 6)})
    if selected != expected_rewards or applied != expected_final_commits:
        problems.append(f"Reward count mismatch: selected={dict(selected)}, applied={dict(applied)}")
    if not re.search(r"PGCombatCycle COMPLETE rewards=7\b", log):
        problems.append("Missing full-run completion with 7 rewards")
    if re.search(r"PGCombatCycle (?:FAILED|TIMEOUT|reward rejected)", log):
        problems.append("Progression probe reported failure")
    return problems


def check_loot_cycle(log):
    """Version 1 authored encounter counts: four elites and one final boss."""
    rolls = re.findall(r'PGLoot rolled enemy=(\d+) pool=(\S+) seed=(-?\d+) item=(\d+) guid=([0-9A-Fa-f]+) result=([01])', log)
    problems = []
    guaranteed = Counter(int(row[0]) for row in rolls if int(row[0]) >= 15104)
    if guaranteed != Counter({15104: 2, 15105: 2, 15106: 1}):
        problems.append(f'Guaranteed loot count mismatch: {dict(guaranteed)}')
    if len({row[4] for row in rolls}) != len(rolls):
        problems.append('Duplicate loot identity spawned')
    boss = [row for row in rolls if row[0] == '15106']
    committed = re.findall(r'PGLoot boss committed item=(\d+) guid=([0-9A-Fa-f]+) wins=(\d+)', log)
    if len(boss) != 1 or boss[0][5] != '1' or len(committed) != 1 or committed[0][:2] != (boss[0][3], boss[0][4]) or committed[0][2] != '1':
        problems.append('Boss loot and victory were not committed exactly once together')
    return problems


def check_retry(log):
    rows = re.findall(r"PGRetryProbe remaining=(\d+) healthy=(\d+) failures=(\d+)", log)
    expected = [(str(n), "1", "0") for n in range(20, -1, -1)]
    problems = []
    if rows != expected:
        problems.append("Expected 20 restarts with 21 healthy observations and zero failures")
    if "PGRetryProbe COMPLETE failures=0" not in log:
        problems.append("Missing restart completion")
    deaths = sum(bool(EXPECTED_DEATH.search(line.strip())) for line in log.splitlines())
    if deaths != 20:
        problems.append(f"Expected 20 injected player deaths, found {deaths}")
    return problems


def check_telemetry(data):
    problems = []
    stages = data.get("stages", [])
    if data.get("schema_version") != 1 or [r.get("stage") for r in stages] != list(range(1, 7)):
        return ["Telemetry must contain schema 1 and exactly six ordered stages"]
    seeds = {r.get("run_seed") for r in stages}
    if len(seeds) != 1 or not all(isinstance(seed, int) and seed > 0 for seed in seeds):
        problems.append("Telemetry has missing or inconsistent run seeds")
    for stage, count in zip(stages, [1, 2, 1, 2, 1, 0]):
        if stage.get("assisted") is not True or stage.get("outcome") != "Cleared":
            problems.append("Automated cycle must be Assisted and Cleared")
        rewards = stage.get("selected_rewards", [])
        if len(rewards) != count or not all(isinstance(r, int) and r > 0 for r in rewards):
            problems.append("Telemetry committed reward IDs are missing")
        for key in ("combat_seconds", "direct_damage", "secondary_damage", "damage_taken"):
            value = stage.get(key)
            if not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0 or (key == "combat_seconds" and value == 0):
                problems.append(f"Invalid telemetry field: {key}")
    return problems


def unexpected_errors(log, gate_name):
    return [line.strip() for line in log.splitlines() if re.search(r"Log\w+: Error:", line)
            and not (gate_name == "retry" and EXPECTED_DEATH.search(line.strip()))]


def stop_child(process):
    """Stop only the process tree started by this runner, never other editor sessions."""
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(["taskkill.exe", "/PID", str(process.pid), "/T", "/F"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       creationflags=subprocess.CREATE_NO_WINDOW, timeout=30)
    else:
        process.kill()
    process.wait(timeout=30)


def run_process(command, cwd, output, timeout):
    with output.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen([str(arg) for arg in command], cwd=cwd, stdout=stream,
                                   stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        try:
            return process.wait(timeout=timeout), False
        except subprocess.TimeoutExpired:
            stop_child(process)
            return process.returncode, True
        except BaseException:
            stop_child(process)
            raise


def git_info(path):
    result = {}
    for key, args in (("commit", ["rev-parse", "HEAD"]), ("status", ["status", "--short"])):
        try:
            process = subprocess.run(["git", "-c", f"safe.directory={path.as_posix()}", "-C", str(path), *args],
                                     capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20)
            result[key] = process.stdout.strip() if process.returncode == 0 else "unavailable"
        except (OSError, subprocess.TimeoutExpired):
            result[key] = "unavailable"
    return result


def write_report(directory, report):
    (directory / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    lines = [f"# UPlayground QA: {report['status']}", "", f"Run: `{report['run_id']}`",
             f"Suite: `{report['suite']}`", "", "| Gate | Result | Seconds | Evidence |",
             "|---|---|---:|---|"]
    for gate in report["gates"]:
        evidence = f"[log]({gate['log']})" if gate.get("log") else "—"
        lines.append(f"| {gate['name']} | {gate['status']} | {gate.get('seconds', 0):.1f} | {evidence} |")
    for gate in report["gates"]:
        for problem in gate.get("problems", []):
            lines.extend(["", f"- **{gate['name']}**: {problem}"])
    if report.get("warnings"):
        lines.extend(["", "## Warnings", ""] + [f"- {warning}" for warning in report["warnings"]])
    lines.extend(["", "## Not covered", "", *[f"- {item}" for item in report["not_covered"]]])
    (directory / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path)
    parser.add_argument("--suite", choices=("quick", "full"), default="full")
    parser.add_argument("--skip-build", action="store_true", help="Use existing editor binaries; report build as SKIPPED")
    args = parser.parse_args()
    project = ROOT / "UPlayground.uproject"
    association = json.loads(read_text(project))["EngineAssociation"]
    engine = (args.engine_root or Path(os.environ.get("ProgramFiles", "C:/Program Files")) /
              "Epic Games" / f"UE_{association}").resolve()
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:8]
    directory = ROOT / "Saved" / "QA" / run_id
    directory.mkdir(parents=True, exist_ok=False)
    report = dict(run_id=run_id, suite=args.suite, status="RUNNING", engine=str(engine),
                  project_engine_association=association,
                  engine_version=json.loads(read_text(engine / "Engine/Build/Build.version"))
                  if (engine / "Engine/Build/Build.version").is_file() else None,
                  source=git_info(ROOT),
                  content=git_info(ROOT / "Content"), gates=[], warnings=[], not_covered=[
                      "Real-input combat, three-build balance, game feel and difficulty",
                      "Rendered UI, VFX/SFX, telegraph readability and screenshot review",
                      "Packaged/cooked executable, GPU performance and 20-minute soak",
                      "Process-to-process checkpoint resume (serialization/save transactions are tested)"])
    if args.suite == "quick":
        report["not_covered"].append("Full-run progression and 20 in-game restarts (use --suite full)")
    if args.skip_build:
        report["not_covered"].append("Build freshness: --skip-build uses existing binaries")
    write_report(directory, report)
    print(f"QA evidence: {directory}", flush=True)

    def gate(name, command, timeout, validator=None, cwd=ROOT, engine_log=None):
        print(f"[{name}] started", flush=True)
        started = time.monotonic()
        output = directory / f"{name}.stdout.log"
        entry = dict(name=name, status="RUNNING", command=[str(arg) for arg in command],
                     log=(engine_log or output).name, problems=[])
        report["gates"].append(entry)
        write_report(directory, report)
        try:
            code, timed_out = run_process(command, cwd, output, timeout)
            entry.update(exit_code=code, timed_out=timed_out)
            if timed_out:
                entry["problems"].append(f"Timed out after {timeout}s")
            if code != 0:
                entry["problems"].append(f"Process exit code: {code}")
            log = read_text(engine_log) if engine_log and engine_log.is_file() else read_text(output)
            if engine_log and not engine_log.is_file():
                entry["problems"].append("Engine did not create its expected log")
                entry["log"] = output.name
            if FATAL.search(log):
                entry["problems"].append("Crash, assertion or ensure found in log")
            if name != "automation":
                errors = unexpected_errors(log, name)
                entry["problems"].extend(errors[:12])
                warning_lines = sorted(set(line.strip() for line in log.splitlines()
                                           if re.search(r"\bwarning\b", line, re.I)))
                entry["warning_count"] = len(warning_lines)
                report["warnings"].extend(f"{name}: {line}" for line in warning_lines[:8])
                if len(warning_lines) > 8:
                    report["warnings"].append(f"{name}: {len(warning_lines) - 8} more warning lines; see log")
            if validator:
                problems, warnings = validator(log)
                entry["problems"].extend(problems)
                report["warnings"].extend(warnings)
        except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
            entry["problems"].append(str(error))
        except KeyboardInterrupt:
            entry["problems"].append("Interrupted by user")
            raise
        finally:
            entry["seconds"] = round(time.monotonic() - started, 2)
            entry["status"] = "FAIL" if entry["problems"] else "PASS"
            write_report(directory, report)
            print(f"[{name}] {entry['status']} ({entry['seconds']}s)", flush=True)
            for problem in entry["problems"]:
                print(f"  {problem}", flush=True)
        return entry["status"] == "PASS"

    editor = engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"

    def ue(name, extra):
        log = directory / f"{name}.log"
        return [editor, project, *extra, "-unattended", "-nop4", "-nosplash", "-NullRHI",
                "-nosound", "-UTF8Output", f"-abslog={log}", f"-PGTestProfile=QA_{run_id}_{name}"], log

    def automation_result(log):
        report_path = directory / "Automation/index.json"
        if not report_path.is_file():
            return ["Automation report missing"], []
        expected = []
        for source in (ROOT / "Source").rglob("*.cpp"):
            expected.extend(re.findall(r'IMPLEMENT_\w+_AUTOMATION_TEST\s*\(\s*\w+\s*,\s*"(PG\.[^"]+)"', read_text(source)))
        if not expected:
            return ["Could not discover expected PG tests from source"], []
        data = json.loads(read_text(report_path))
        report["automation_counts"] = {key: data.get(key) for key in
                                       ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess")}
        return check_automation(data, expected)

    def asset_result(log):
        if "PGRogue VALIDATION PASS " not in log:
            return ["Missing current-run asset validation success marker"], []
        return [], []

    def cycle_result(log):
        problems = check_cycle(log) + check_loot_cycle(log)
        paths = set(re.findall(r"PGRun stage=\d+ seed=\d+ assisted=\d telemetry=(.+)", log))
        if len(paths) != 1:
            return problems + ["Missing unique current-cycle telemetry file"], []
        path = (ROOT / next(iter(paths)).strip()).resolve()
        if not path.is_relative_to((ROOT / "Saved/RunTelemetry").resolve()) or not path.is_file():
            return problems + ["Current-cycle telemetry file is missing or outside Saved/RunTelemetry"], []
        try:
            data = json.loads(read_text(path))
            problems.extend(check_telemetry(data))
            (directory / "cycle.telemetry.json").write_text(json.dumps(data, indent=2), encoding="utf-8")
        except (OSError, ValueError, TypeError) as error:
            problems.append(f"Unreadable telemetry: {error}")
        return problems, []

    try:
        if not editor.is_file():
            raise FileNotFoundError(f"Unreal editor not found: {editor}; specify --engine-root")
        if args.skip_build:
            report["gates"].append(dict(name="build", status="SKIPPED"))
        else:
            dotnet = sorted((engine / "Engine/Binaries/ThirdParty/DotNet").glob("*/win-x64/dotnet.exe"))
            if len(dotnet) != 1:
                raise RuntimeError("Expected one bundled Windows x64 .NET SDK")
            ubt = engine / "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
            if not gate("build", [dotnet[0], ubt, "UPlaygroundEditor", "Win64", "Development",
                                  f"-Project={project}", "-WaitMutex", "-NoHotReloadFromIDE",
                                  f"-Log={directory / 'build.log'}"], 1800,
                        cwd=engine / "Engine/Source", engine_log=directory / "build.log"):
                raise RuntimeError("Build failed; dependent checks were not run")

        command, log = ue("automation", ["-ExecCmds=Automation RunTests PG.",
                                        "-TestExit=Automation Test Queue Empty",
                                        f"-ReportExportPath={directory / 'Automation'}"])
        gate("automation", command, 600, automation_result, engine_log=log)
        command, log = ue("assets", ["-EnablePlugins=PythonScriptPlugin", "-run=pythonscript",
                                    f"-script={ROOT / 'Tools/Validation/ValidateRoguelikeMVP.py'}"])
        assets_ok = gate("assets", command, 600, asset_result, engine_log=log)
        if args.suite == "full":
            if assets_ok:
                command, log = ue("cycle", ["/Game/Maps/RogueArena", "-game", "-PGRogueAutoStart",
                                           "-PGRunSeed=173001",
                                           "-ExecCmds=t.MaxFPS 60,PGCombatCycleSmoke", "-seconds=240",
                                           "-TestExit=PGCombatCycle COMPLETE+PGCombatCycle FAILED+PGCombatCycle TIMEOUT"])
                gate("cycle", command, 600, cycle_result, engine_log=log)
                command, log = ue("retry", ["/Game/Maps/RogueArena", "-game", "-PGRogueAutoStart", "-PGRetryProbe=20"])
                gate("retry", command, 600, lambda text: (check_retry(text), []), engine_log=log)
            else:
                for name in ("cycle", "retry"):
                    report["gates"].append(dict(name=name, status="BLOCKED", problems=["Asset validation failed"]))
    except (OSError, RuntimeError, KeyboardInterrupt) as error:
        report["gates"].append(dict(name="runner", status="FAIL", problems=[str(error) or "Interrupted"]))
    finally:
        failed = any(gate["status"] in ("FAIL", "BLOCKED") for gate in report["gates"])
        report["status"] = "FAIL" if failed else "PASS_WITH_WARNINGS" if report["warnings"] else "PASS"
        write_report(directory, report)
        print(f"QA {report['status']}: {directory / 'report.md'}", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

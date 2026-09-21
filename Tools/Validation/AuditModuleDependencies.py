"""Emit the declared project-module graph and concrete includes for review."""
from pathlib import Path
import re
import json

root = Path(__file__).resolve().parents[2]
modules = {p.parent.name: p for p in (root / "Source").glob("*/*.Build.cs")}
result = {}
for name, build in modules.items():
    text = build.read_text(encoding="utf-8-sig")
    dependencies = sorted(set(re.findall(r'"(PG\w+|UPlayground)"', text)) - {name})
    includes = {}
    for source in build.parent.rglob("*"):
        if source.suffix not in (".h", ".cpp"):
            continue
        for dependency in re.findall(r'#include\s+"(PG\w+|UPlayground)/', source.read_text(encoding="utf-8-sig")):
            if dependency != name:
                includes.setdefault(dependency, []).append(str(source.relative_to(root)))
    result[name] = {"declared": dependencies, "includes": {k: sorted(set(v)) for k, v in includes.items()}}
assert "PGAbilitySystem" not in result["PGData"]["declared"]
assert "PGAbilitySystem" not in result["PGData"]["includes"]
assert "PGAbilitySystem" not in result["PGShared"]["declared"]
assert "PGAbilitySystem" not in result["PGShared"]["includes"]
out = root / "Saved/Automation/Stage34/ModuleDependencies.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(out)

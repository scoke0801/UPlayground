"""Read saved profile FX and every swing phase without changing assets."""
import json
from pathlib import Path
import sys
import unreal
sys.path.insert(0, str(Path(__file__).resolve().parent))
from PlayerNiagaraData import profiles

result = {}
for skill, profile in profiles().items():
    fx = profile.get_editor_property('slash_vfx')
    result[skill] = dict(system=fx.get_path_name() if fx else None,
                        duration=profile.get_editor_property('slash_duration'),
                        phases=[p.export_text() for p in profile.get_editor_property('hit_phases')])
out = Path(unreal.Paths.project_dir())/'Saved/QA/PlayerSwingFX_Before.json'
out.write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log('PGPlayerSwingFX AUDIT PASS '+str(out))

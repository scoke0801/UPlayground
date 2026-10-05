"""Fresh-process verification of saved Niagara systems and unchanged gameplay data."""
import hashlib
import json
from pathlib import Path
import sys
import unreal
sys.path.insert(0, str(Path(__file__).resolve().parent))
from PlayerNiagaraData import SYSTEMS, profiles, snapshot, has_projectile

root = Path(unreal.Paths.project_dir()).resolve()
backup = Path((root/'Saved/PlayerNiagara_LastBackup.txt').read_text(encoding='utf-8').strip())
before = json.loads((backup/'before.json').read_text(encoding='utf-8'))
source = json.loads((backup/'source.json').read_text(encoding='utf-8'))
assert hashlib.sha256((root/'Content'/(source['path'].removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest() == source['sha256']
for path in SYSTEMS.values():
    asset = unreal.load_asset(path)
    assert isinstance(asset, unreal.NiagaraSystem), path
    description = json.loads(unreal.PGNiagaraFXTools.describe_system(asset))
    assert description['compiled'], path
    assert any(p['name'] == 'User.SlashTint' for p in description['user']), description
    assert any(p['name'] == 'User.SlashAlpha' for p in description['user']), description
    enabled = [e for e in description['emitters'] if e['enabled']]
    assert len(enabled) >= 2 and all(e['local_space'] for e in enabled), description
table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
rows = {row['SkillID']: row for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
for skill, asset in profiles().items():
    assert rows[skill]['PlayerProfile'] == asset.get_path_name(), (skill, rows[skill]['PlayerProfile'])
    assert snapshot(asset) == before[str(skill)], skill
    projectile = has_projectile(asset)
    assert asset.get_editor_property('slash_vfx').get_path_name().split('.')[0] == SYSTEMS['blade' if projectile else 'slash'], skill
    swing = asset.get_editor_property('projectile_swing_vfx')
    assert (swing.get_path_name().split('.')[0] if swing else None) == (SYSTEMS['swing'] if projectile else None), skill
    assert asset.get_editor_property('projectile_swing_radius') == 250., skill
    assert asset.get_editor_property('niagara_reference_radius') > 0
unreal.log('PGPlayerNiagara VALIDATION PASS systems=3 profiles=8 source_unchanged=1 gameplay_unchanged=1')

"""Narrow, SHA-256 backed-up edits to startup dodge and full-body travel speed."""
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterTransaction import Transaction, write_json

spec = json.loads((ROOT/'Tools/Validation/HackSlashP0.json').read_text(encoding='utf-8'))
items = [s for s in spec['skills'] if s['id'] in (100, 101, 102, 112)]
assets = {s['id']: unreal.load_asset('/Game/DataCenter/HackSlashP0/DA_PlayerSkill_'+str(s['id'])) for s in items}
assert all(assets.values())
marker = ROOT/'Saved/FocusedCombat_LastBackup.txt'
validate = '-PGFocusedValidate' in unreal.SystemLibrary.get_command_line()
if not validate:
    out = ROOT/'Saved/Backups/FocusedCombat'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    transaction = Transaction(ROOT, out)
    transaction.prepare([a.get_path_name().split('.')[0] for a in assets.values()], Path(__file__))
    marker.write_text(str(out), encoding='utf-8')
    before = {}
    for item in items:
        asset = assets[item['id']]
        before[item['id']] = dict(early_dodge_until=asset.get_editor_property('early_dodge_until'),
            moves=[m.export_text() for m in asset.get_editor_property('movement_segments')])
        if item['id'] != 112:
            asset.set_editor_property('early_dodge_until', item['early_dodge'])
        else:
            moves = list(asset.get_editor_property('movement_segments'))
            assert len(moves) == 1 and moves[0].mode == unreal.PGPlayerMoveMode.WALK
            moves[0].walk_speed_ratio = item['walk_speed_ratio']
            asset.set_editor_property('movement_segments', moves)
        transaction.mark_written(asset.get_path_name().split('.')[0])
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    write_json(out/'before.json', before)
    transaction.data['status'] = 'APPLIED_UNVERIFIED'; transaction.flush()
for item in items:
    asset = assets[item['id']]
    assert abs(asset.get_editor_property('early_dodge_until')-item.get('early_dodge', 0.)) < .0001
    assert abs(asset.get_editor_property('movement_segments')[0].walk_speed_ratio-item['walk_speed_ratio']) < .0001
if validate:
    out = Path(marker.read_text(encoding='utf-8'))
    manifest = json.loads((out/'transaction.json').read_text(encoding='utf-8'))
    assert manifest['status'] in ('APPLIED_UNVERIFIED', 'VERIFIED')
    manifest['status'] = 'VERIFIED'; write_json(out/'transaction.json', manifest)
unreal.log('PGFocusedCombat '+('VALIDATION' if validate else 'APPLY')+' PASS')

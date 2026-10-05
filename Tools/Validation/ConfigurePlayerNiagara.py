"""Clone authored FX into project-owned systems, bind tint, and connect eight profiles."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import sys
import unreal
sys.path.insert(0, str(Path(__file__).resolve().parent))
from PlayerNiagaraData import SOURCE, SYSTEMS, profiles, snapshot, has_projectile

root = Path(unreal.Paths.project_dir()).resolve()
backup = root/'Saved/Backups/PlayerNiagara'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
backup.mkdir(parents=True)
assets = profiles()
before = {str(skill): snapshot(asset) for skill, asset in assets.items()}
(backup/'before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
for path in [a.get_path_name().split('.')[0] for a in assets.values()] + list(SYSTEMS.values()):
    relative = path.removeprefix('/Game/')+'.uasset'
    file = root/'Content'/relative
    if file.exists():
        target = backup/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
source_file = root/'Content'/(SOURCE.removeprefix('/Game/')+'.uasset')
source_hash = hashlib.sha256(source_file.read_bytes()).hexdigest()
systems = {}
for kind, path in SYSTEMS.items():
    system = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not system:
        system = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, path)
        assert system, path
    assert unreal.PGNiagaraFXTools.configure_slash(system), path
    description = json.loads(unreal.PGNiagaraFXTools.describe_system(system))
    assert any(p['name'] == 'User.SlashTint' for p in description['user']), description
    assert all(e['local_space'] for e in description['emitters'] if e['enabled']), description
    assert unreal.EditorAssetLibrary.save_loaded_asset(system, only_if_is_dirty=False)
    systems[kind] = system
    (backup/(kind+'_system.json')).write_text(json.dumps(description, indent=2), encoding='utf-8')
for skill, asset in assets.items():
    projectile = has_projectile(asset)
    asset.set_editor_property('slash_vfx', systems['blade' if projectile else 'slash'])
    asset.set_editor_property('projectile_swing_vfx', systems['swing'] if projectile else None)
    asset.set_editor_property('projectile_swing_radius', 250.)
    asset.set_editor_property('niagara_reference_radius', 200.)
    # The original mesh's readable sweep occupies the first .24s of its .4s lifetime.
    # Runtime applies a separate smooth end fade to every layer.
    asset.set_editor_property('niagara_reference_duration', .24)
    asset.set_editor_property('niagara_rotation', unreal.Rotator(0., 0., 0.))
    assert snapshot(asset) == before[str(skill)], skill
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
assert hashlib.sha256(source_file.read_bytes()).hexdigest() == source_hash
(backup/'before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
(backup/'source.json').write_text(json.dumps(dict(path=SOURCE, sha256=source_hash)), encoding='utf-8')
(root/'Saved/PlayerNiagara_LastBackup.txt').write_text(str(backup), encoding='utf-8')
unreal.log('PGPlayerNiagara APPLY PASS systems=3 profiles=8 gameplay_unchanged=1 backup='+str(backup))

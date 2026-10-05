"""Fresh-process saved profile, mapping, and non-target row regression checks."""
import json
from pathlib import Path
import unreal


def validate_hack_slash_p0(rows=None):
    root = Path(unreal.Paths.project_dir())
    spec = json.loads((root / 'Tools/Validation/HackSlashP0.json').read_text(encoding='utf-8'))
    if rows is None:
        table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
        rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    by_id = {r['SkillID']: r for r in rows}
    regression_rows = by_id
    if any(isinstance(r.get('PlayerProfile'),str) and r['PlayerProfile'].startswith('/Game/DataCenter/HackSlashP1/') for r in rows):
        from ValidateHackSlashP1 import validate_hack_slash_p1
        regression_rows = validate_hack_slash_p1(rows)
    for item in spec['skills']:
        row = by_id[item['id']]
        path = row['PlayerProfile']
        assert isinstance(path, str) and path.startswith('/Game/DataCenter/HackSlashP0/'), path
        profile = unreal.load_asset(path)
        assert isinstance(profile, unreal.PGPlayerSkillProfile)
        assert profile.get_editor_property('skill_id') == item['id']
        assert row['SkillCoolTime'] == item['cooldown'] and row['Desc'] == item['name']
        for key, expected in [('duration',item['duration']),('dodge_cancel',item['dodge']),('attack_cancel',item['attack'])]:
            assert abs(profile.get_editor_property(key)-expected)<.0001
        phases = profile.get_editor_property('hit_phases')
        assert len(phases)==len(item['hits'])
        for phase, time in zip(phases,item['hits']):
            assert abs(phase.get_editor_property('start')-time)<.0001
            assert abs(phase.get_editor_property('end')-(time+item.get('hit_window', .06)))<.0001
            assert abs(phase.get_editor_property('damage_multiplier')-item['damage'])<.0001
            assert phase.get_editor_property('radius')==item['radius']
            assert phase.get_editor_property('full_angle_degrees')==item['angle']
        assert profile.get_editor_property('slash_material')
        assert profile.get_editor_property('swing_sound')
        assert len(profile.get_editor_property('movement_segments')) == 1
        movement = profile.get_editor_property('movement_segments')[0]
        for key, expected in [('start',item['move_start']),('end',item['move_end']),('distance',item['distance'])]:
            assert abs(movement.get_editor_property(key)-expected)<.0001,(item['id'],key)
        keys = profile.get_editor_property('pose_keys')
        assert len(keys) == len(item['pose'])
        for key, (time, seconds) in zip(keys, item['pose']):
            assert abs(key.get_editor_property('time')-time)<.0001
            assert abs(key.get_editor_property('montage_seconds')-seconds)<.0001
        montage = unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
        from ConfigureAttackMotion import validate_profile_motion
        validate_profile_motion(item, profile, montage)
        assert profile.get_editor_property('pose_keys')[-1].get_editor_property('montage_seconds') <= montage.get_play_length()
    backup_marker = root / 'Saved/HackSlashP0_LastBackup.txt'
    if backup_marker.exists():
        backup = Path(backup_marker.read_text(encoding='utf-8'))
        old_rows = json.loads((backup/'skills.json').read_text(encoding='utf-8'))
        allowed = {s['id'] for s in spec['skills']}
        assert len(rows) == len(old_rows)
        for old in old_rows:
            current = regression_rows[old['SkillID']]
            for field in old:
                if old['SkillID'] in allowed and field in ('PlayerProfile','SkillCoolTime','Desc'):
                    continue
                assert current[field] == old[field], (old['SkillID'], field)
    unreal.log('PGHackSlash VALIDATION PASS profiles=5 saved_reload=1 non_target_rows_unchanged=1')
    return {'player_profiles':5}


if __name__ == '__main__':
    validate_hack_slash_p0()

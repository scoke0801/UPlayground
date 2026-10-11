"""Read saved enemy rows and montage segments without modifying assets."""
import json
import os
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = Path(os.environ.get('PG_ENEMY_TEMPO_RUN', str(root/'Saved/EnemyMotionTempo')))
out.mkdir(parents=True, exist_ok=True)
table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
records = []
for row in rows:
    if row['SkillID'] < 15000 or not row.get('bSyncMontageToPattern'):
        continue
    montage = unreal.load_asset(row['ElitePresentationMontage'])
    if not montage:
        continue
    length = montage.get_play_length()
    segments = []
    for track in montage.get_editor_property('slot_anim_tracks'):
        for segment in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
            clip = segment.get_editor_property('anim_reference')
            segments.append(dict(clip=clip.get_path_name(), length=clip.get_play_length(),
                                 start=segment.get_editor_property('anim_start_time'),
                                 end=segment.get_editor_property('anim_end_time')))
    records.append(dict(row=row, length=length, segments=segments,
                        windup_rate=length*(row['ImpactMontageFraction']-row['WindupMontageFraction'])/row['TelegraphDuration'],
                        recovery_rate=length*(1-row['ImpactMontageFraction'])/max(.05,row['RecoveryDuration'])))
(out/'inspection.json').write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PGEnemyTempo INSPECT PASS '+str(len(records)))

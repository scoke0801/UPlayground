"""Shared authored extension contract for the existing content validators."""
import json
from pathlib import Path
import unreal
SPEC=json.loads((Path(unreal.Paths.project_dir())/'Tools/Validation/Data/MotionExpansion.json').read_text(encoding='utf-8'))

def installed():
    return unreal.EditorAssetLibrary.does_asset_exist('/Game/DataCenter/MotionExpansion/Player/DA_116')

def additional_ids():
    return {x['id'] for x in SPEC['player']+SPEC['enemy_skills']} if installed() else set()

def enemy_skills(eid,baseline):
    return list(dict.fromkeys(list(baseline)+([x['id'] for x in SPEC['enemy_skills'] if x['enemy']==eid] if installed() else [])))

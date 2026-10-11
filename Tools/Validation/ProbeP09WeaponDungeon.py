"""Reuse dungeon lifecycle acceptance, adding direct actor-ID spawn evidence."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(unreal.Paths.project_dir()).resolve()/'Tools/Validation'))
import ProbeDungeonCombat as probe
from P09WeaponRoster import IDS

original_actors=probe.actors


def observed_actors(world):
    result=original_actors(world)
    observed=set(probe.report.get('p09_weapon_ids',[]))
    observed.update(a.get_editor_property('character_tid') for a in result if a.get_editor_property('character_tid') in IDS)
    probe.report['p09_weapon_ids']=sorted(observed)
    return result


probe.actors=observed_actors

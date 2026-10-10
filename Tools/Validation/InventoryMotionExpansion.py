"""Disk-only planning inventory. Does not load UE or prove runtime non-use."""
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Docs/todo/assets/MotionExpansion'


def read(relative):
    return json.loads((ROOT / relative).read_text(encoding='utf-8-sig'))


def package_file(package):
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')


def main():
    entries = {}
    for relative in ('Docs/Design/MotionBasedSkills/motion-manifest.json',
                     'Docs/Design/HumanoidBoss/motion-manifest.json'):
        for item in read(relative)['motions']:
            for field in ('asset', 'source'):
                path = item.get(field)
                if not path or not path.startswith('/Game/'):
                    continue
                entry = entries.setdefault(path, dict(asset=path, evidence=[],
                    status='historical_design_candidate_or_baseline_requires_runtime_audit'))
                entry['evidence'].append(relative + ':' + field)

    spec = read('Tools/Validation/Data/CreatureCombat.json')
    creature_summary = {}
    for monster in spec['monsters']:
        model = monster['model']
        uses = {monster['idle']: ['idle'], monster['death']: ['death'], 'Hit': ['hit_react']}
        uses.setdefault(monster['move'], []).append('move')
        for skill in spec['skills']:
            if skill['model'] == model:
                uses.setdefault(skill['clip'], []).append('skill:' + str(skill['id']))
        paths = sorted((ROOT / 'Content/Art/CreatureModels' / model / 'Animations').glob('AS_*.uasset'))
        linked = 0
        for path in paths:
            clip = path.stem.removeprefix('AS_PG_' + model + '_')
            roles = uses.get(clip, [])
            linked += bool(roles)
            asset = '/Game/' + path.relative_to(ROOT / 'Content').with_suffix('').as_posix()
            entries[asset] = dict(asset=asset,
                status='authored_combat_link' if roles else 'not_linked_by_creature_combat_config',
                evidence=['Tools/Validation/Data/CreatureCombat.json',
                          'Tools/Validation/ConfigureCreatureCombat.py'] + roles)
        creature_summary[model] = dict(sequence_files=len(paths), authored_linked=linked,
                                      not_linked_by_this_config=len(paths) - linked)

    for entry in entries.values():
        entry['disk_exists'] = package_file(entry['asset']).is_file()
        entry['evidence'] = '; '.join(sorted(set(entry['evidence'])))
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / 'inventory.csv').open('w', encoding='utf-8-sig', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=['asset', 'disk_exists', 'status', 'evidence'])
        writer.writeheader()
        writer.writerows(entries[key] for key in sorted(entries))
    summary = dict(scope='two historical candidate manifests plus all four imported creature sequence folders',
        verification='disk existence and authored creature bindings only; no AssetRegistry, UE load, playback or runtime usage audit',
        packages=len(entries), missing=[key for key, value in entries.items() if not value['disk_exists']],
        creatures=creature_summary)
    (OUT / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()

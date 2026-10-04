"""Read-only validation in a fresh editor; also part of the regular asset gate."""
import unreal


def validate_combat_bt(enemies):
    trees = set()
    for enemy_id in range(15101, 15107):
        row = enemies[enemy_id]
        tree = unreal.load_asset(row['CombatBehaviorTree'])
        assert tree and unreal.PGCombatTreeLibrary.validate_combat_tree(tree, True), f'{enemy_id}: invalid editable combat BT'
        trees.add(tree.get_path_name())
        tuning = row['Positioning']
        assert tuning['bEnabled'] and .5 <= tuning['ReconsiderSeconds'] <= 10
        assert 60 <= tuning['Separation'] <= 500 and 60 <= tuning['MaxMoveDistance'] <= 800
        assert tuning['MinimumImprovement'] >= 10
    return dict(schema=1, roles=6, editable_trees=len(trees), positioning=True)

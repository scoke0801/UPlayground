"""Create the initial farming catalog once; never overwrite subsequent designer tuning."""
import unreal

path = "/Game/DataCenter/Progression/DA_PGProgression"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.PGProgressionData)
    data = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_PGProgression", "/Game/DataCenter/Progression", unreal.PGProgressionData, factory)
    items = []
    for item_id, name, slot, rarity, options, weight, roll in [
        (3401, "수련자의 칼날", unreal.PGEquipmentSlot.WEAPON, unreal.PGItemRarity.COMMON, {unreal.PGStatType.ATTACK: 20}, 5, 5),
        (3402, "연격의 칼날", unreal.PGEquipmentSlot.WEAPON, unreal.PGItemRarity.MAGIC, {unreal.PGStatType.ATTACK: 45, unreal.PGStatType.CRITICAL_RATE: 500}, 3, 10),
        (3403, "강타의 칼날", unreal.PGEquipmentSlot.WEAPON, unreal.PGItemRarity.RARE, {unreal.PGStatType.ATTACK: 90}, 1, 20),
        (3404, "수호의 부적", unreal.PGEquipmentSlot.ACCESSORY, unreal.PGItemRarity.MAGIC, {unreal.PGStatType.DEFENSE: 120, unreal.PGStatType.HEALTH: 100}, 3, 15),
    ]:
        item = unreal.PGItemDataRow()
        for key, value in dict(id=item_id, display_name=name, slot=slot, rarity=rarity, base_options=options, drop_weight=weight, roll_bonus=roll).items():
            item.set_editor_property(key, value)
        items.append(item)
    data.set_editor_property("items", items)
    builds = []
    slots = [unreal.PGSkillSlot.NORMAL_ATTACK, unreal.PGSkillSlot.SKILL_SLOT_1, unreal.PGSkillSlot.SKILL_SLOT_2,
             unreal.PGSkillSlot.SKILL_SLOT_3, unreal.PGSkillSlot.SKILL_SLOT_4, unreal.PGSkillSlot.SKILL_SLOT_5,
             unreal.PGSkillSlot.SKILL_SLOT_6, unreal.PGSkillSlot.SKILL_SLOT_ROLL, unreal.PGSkillSlot.SKILL_SLOT_JUMP]
    for build_id, name, clears, scale, ids in [
        ("Rapid1", "연격 I — 빠른 스킬 순환", 0, .7, [100,113,111,112,110,114,115,10000,20000]),
        ("Heavy1", "강타 I — 다른 액티브 구성", 0, 1., [100,112,115,113,110,114,111,10000,20000]),
        ("Rapid2", "연격 II — 쿨다운 개선", 1, .55, [100,113,111,112,110,114,115,10000,20000]),
        ("Heavy2", "강타 II — 쿨다운 개선", 1, .8, [100,112,115,113,110,114,111,10000,20000]),
    ]:
        build = unreal.PGBuildDefinition()
        build.set_editor_property("id", build_id)
        build.set_editor_property("display_name", name)
        build.set_editor_property("required_clears", clears)
        entries = []
        for index, (slot, skill_id) in enumerate(zip(slots, ids)):
            entry = unreal.PGLoadoutEntry()
            entry.set_editor_property("slot", slot)
            entry.set_editor_property("skill_id", skill_id)
            entry.set_editor_property("cooldown_seconds", (12. if build_id.startswith("Heavy") and index == 2 else 8.) if slot in slots[1:7] else -1.)
            entry.set_editor_property("cooldown_scale", scale if slot in slots[1:7] else 1.)
            entries.append(entry)
        build.set_editor_property("skills", entries)
        builds.append(build)
    data.set_editor_property("builds", builds)
    if not unreal.EditorAssetLibrary.save_loaded_asset(data):
        raise RuntimeError("Catalog save failed")
unreal.log("PG Stage34 farming catalog ready")

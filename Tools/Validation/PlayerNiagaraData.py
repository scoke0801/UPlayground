"""Shared read-only snapshot helpers for Niagara-only player presentation migration."""
import unreal

SKILLS = (100, 101, 102, 110, 111, 112, 113, 114)
DEST = '/Game/Art/PlayerCombatFX'
SYSTEMS = {'slash': DEST+'/NS_PGPlayerSlash', 'blade': DEST+'/NS_PGPlayerBlade',
           'swing': DEST+'/NS_PGPlayerCastSwing'}
SOURCE = '/Game/ExternalAssets/VFX/MixedVFX/Particles/Slashes/SeparateParts/Slashes/NS_HolySlash_OnlySlash'


def has_projectile(profile):
    return any(p.get_editor_property('shape') == unreal.PGPlayerHitShape.PROJECTILE
               for p in profile.get_editor_property('hit_phases'))


def profiles():
    result = {}
    for skill in SKILLS:
        folder = 'HackSlashP1' if skill in (110, 113, 114) else 'HackSlashP0'
        asset = unreal.load_asset(f'/Game/DataCenter/{folder}/DA_PlayerSkill_{skill}')
        assert asset and asset.get_editor_property('skill_id') == skill, skill
        result[skill] = asset
    return result


def snapshot(profile):
    result = {}
    names = ('skill_id', 'duration', 'aim_lock', 'dodge_cancel', 'attack_cancel', 'early_dodge_until',
             'projectile_speed', 'projectile_range', 'projectile_lifetime', 'attack_speed',
             'frenzy_per_cast_cap', 'hit_phases', 'movement_segments', 'pose_keys', 'swing_sound',
             'slash_material', 'slash_tint', 'slash_duration', 'slash_width', 'slash_intensity',
             'slash_height', 'reverse_slash')
    for name in names:
        value = profile.get_editor_property(name)
        if name in ('hit_phases', 'movement_segments', 'pose_keys'):
            result[name] = [item.export_text() for item in value]
        elif hasattr(value, 'export_text'):
            result[name] = value.export_text()
        else:
            result[name] = value.get_path_name() if isinstance(value, unreal.Object) else str(value)
    return result

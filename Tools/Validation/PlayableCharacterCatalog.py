"""Stable save identities shared by character authoring and validation tools."""
# Hichi is a persistent save ID. Its current displayed identity/model is Yura.
PLAYER_IDS = ['Bokusei', 'LianLian', 'Honoka', 'Hichi', 'Siuha', 'Lili', 'Nenmir', 'Hwarin', 'Arin']
ENEMY_IDS = ['P09_Female', 'P09_Male', 'P09_Female_Armor007', 'P09_Male_Armor007']
MODEL_REPLACEMENTS = {'Hwarin': ('Honoka', 'Hwarin'), 'Arin': ('Nenmir', 'Arin'), 'Hichi': ('Hichi', 'Yura')}
PORTRAIT_OVERRIDES = {
    'Hichi': 'AdditionalPortraits_20261005/T_Yura_v2.png',
    'Hwarin': 'AdditionalPortraits_20261005/T_Hwarin.png',
    'Arin': 'AdditionalPortraits_20261005/T_Arin.png',
}


def portrait_source(root, identity):
    return root / 'Tools/Art/MoonlitUI' / PORTRAIT_OVERRIDES.get(identity, 'T_' + identity + '.png')

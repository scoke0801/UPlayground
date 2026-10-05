"""Read back saved portrait links, import hashes, alpha and UI settings in fresh UE."""
import hashlib
import json
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterCatalog import PLAYER_IDS, PORTRAIT_OVERRIDES, portrait_source
manifest = json.loads((ROOT/'Tools/Art/MoonlitUI/ModelPortraits.json').read_text(encoding='utf-8'))
originals = {row['name']: row for row in manifest['portraits']}
approved = {row['name']: row for row in json.loads((ROOT/'Tools/Art/MoonlitUI/AdditionalPortraits_20261005/Validation.json').read_text(encoding='utf-8-sig'))}
catalog = unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
actual = {}
for ref in catalog.get_editor_property('playable_characters'):
    appearance = ref if isinstance(ref, unreal.PGCharacterAppearance) else unreal.load_asset(str(ref))
    name = str(appearance.get_editor_property('id'))
    portrait = appearance.get_editor_property('portrait')
    actual[name] = portrait.get_path_name()
assert sorted(actual) == sorted(PLAYER_IDS)
report = dict(status='PASS', portraits=[])
for name in PLAYER_IDS:
    expected = '/Game/DataCenter/UI/Moonlit/T_'+name+'.T_'+name
    assert actual[name] == expected, (name, actual[name])
    texture = unreal.load_asset(expected)
    imported = [str(p) for p in texture.get_editor_property('asset_import_data').extract_filenames()]
    source = portrait_source(ROOT,name)
    assert len(imported) == 1 and Path(imported[0]).resolve() == source.resolve(), imported
    if name in PORTRAIT_OVERRIDES:
        row = approved['Yura' if name == 'Hichi' else name]
        assert source.resolve() == (ROOT/row['file']).resolve()
        assert hashlib.sha256(source.read_bytes()).hexdigest().lower() == row['sha256'].lower()
    else:
        assert hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(Path(originals[name]['source']).read_bytes()).digest()
    assert texture.blueprint_get_size_x() == 1024 and texture.blueprint_get_size_y() == 1536
    assert texture.get_editor_property('compression_no_alpha') is False
    assert texture.get_editor_property('lod_group') == unreal.TextureGroup.TEXTUREGROUP_UI
    assert texture.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_EDITOR_ICON
    report['portraits'].append(dict(name=name, asset=expected, source=str(source), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
(ROOT/'Saved/MoonlitUI/ModelReferences/reload-validation.json').write_text(json.dumps(report, indent=2),encoding='utf-8')
unreal.log(f'MoonlitUI RELOAD PASS portraits={len(PLAYER_IDS)}')

"""Read Unity material semantics without modifying source files or importing Unreal."""
import re
from pathlib import Path


def number(text, key, default):
    match = re.search(r'^\s*- ' + re.escape(key) + r': ([-+0-9.eE]+)\s*$', text, re.M)
    return float(match[1]) if match else default


def texture_guid(text, key):
    match = re.search(r'^\s*- ' + re.escape(key) + r':\s*\n\s*m_Texture: \{[^\n]*guid: ([a-f0-9]{32})', text, re.M)
    return match[1] if match else None


def shader_catalog(project):
    result = {}
    roots = [project / 'Assets', project / 'Packages']
    cache = project / 'Library/PackageCache'
    if cache.exists():
        roots += list(cache.glob('*liltoon*'))
    for root in roots:
        for meta in root.rglob('*.shader.meta'):
            match = re.search(r'^guid: ([a-f0-9]{32})$', meta.read_text(encoding='utf-8-sig'), re.M)
            source = Path(str(meta)[:-5])
            if not match or not source.is_file():
                continue
            name = re.search(r'Shader\s+"([^"]+)"', source.read_text(encoding='utf-8-sig'))
            if name:
                result[match[1]] = {'name': name[1], 'path': str(source)}
    return result


def alpha_semantics(text, shaders):
    guid = re.search(r'm_Shader: \{[^\n]*guid: ([a-f0-9]{32})', text)
    shader = shaders.get(guid[1], {}) if guid else {}
    name = shader.get('name', '')
    # Shader variants own the render mode; queue=-1 means shader default.
    if 'liltoon' in name.lower():
        mode = 'translucent' if any(v in name.lower() for v in ['transparent', 'refraction', 'gem']) else (
            'masked' if 'cutout' in name.lower() else 'opaque')
    else:
        # Existing unknown shaders remain explicit fallbacks, recorded in the audit.
        queue = re.search(r'^  m_CustomRenderQueue: (-?\d+)', text, re.M)
        transparent = ((int(queue[1]) >= 3000 if queue else False) or
                       number(text, '_DstBlend', 0) == 10 or number(text, '_Surface', 0) == 1 or
                       number(text, '_Mode', 0) in [2, 3])
        mode = 'translucent' if transparent else 'masked'
    return {'render_mode': mode, 'shader_name': name, 'shader_source': shader.get('path'),
            'shader_resolved': bool(name), 'use_base_alpha': 0 if mode == 'opaque' else 1}


def parse_material(path, guids, shaders):
    text = path.read_text(encoding='utf-8-sig')
    match = re.search(r'^  m_Name: (.+)$', text, re.M)
    name = match[1].strip() if match else path.stem
    guid = next((g for k in ['_MainTex', '_BaseMap', '_BaseColorMap'] if (g := texture_guid(text, k))), None)
    alpha_guid = texture_guid(text, '_AlphaMask')
    alpha_mode = number(text, '_AlphaMaskMode', 0)
    if not alpha_guid:
        alpha_guid = texture_guid(text, '_ClippingMask')
        if alpha_guid:
            alpha_mode = 1
    tint_match = re.search(r'^\s*- _Color: \{r: ([^,]+), g: ([^,]+), b: ([^,]+), a: ([^}]+)\}', text, re.M)
    tint = [float(v) for v in tint_match.groups()] if tint_match else [1, 1, 1, 1]
    semantics = alpha_semantics(text, shaders)
    opaque = semantics['render_mode'] == 'opaque'
    return dict(semantics, name=name, source_material=str(path), texture=str(guids[guid]) if guid in guids else None,
                texture_guid=guid, opacity_texture=str(guids[alpha_guid]) if alpha_guid in guids else None,
                alpha_mode=0 if opaque else alpha_mode, alpha_scale=number(text, '_AlphaMaskScale', 1),
                alpha_value=number(text, '_AlphaMaskValue', 0), cutoff=number(text, '_Cutoff', .333),
                transparent=semantics['render_mode'] == 'translucent', tint=tint,
                main_opacity=1 if opaque else tint[3])

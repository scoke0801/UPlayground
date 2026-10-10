"""Read-only Niagara compatibility inventory for combat integration."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
paths = [
    '/Game/ExternalAssets/VFX/SlashTrail_SoftTofu/Niagara/Basic/NS_SlashTrail_Basic',
    '/Game/ExternalAssets/VFX/SlashTrail_SoftTofu/Niagara/Basic/NS_Hit_Basic_Once',
    '/Game/ExternalAssets/VFX/SlashTrail_SoftTofu/Niagara/Wind/NS_SlashTrail_Wind',
    '/Game/ExternalAssets/VFX/SwordTrailVFX/VFX/NS_Trail_01',
    '/Game/ExternalAssets/VFX/Niagara/GroundRocks/NS_GroundBurstRocks',
    '/Game/ExternalAssets/VFX/MixedVFX/Particles/Slashes/SeparateParts/Slashes/NS_HolySlash_OnlySlash',
]
result = {}
for path in paths:
    asset = unreal.load_asset(path)
    result[path] = json.loads(unreal.PGNiagaraFXTools.describe_system(asset)) if asset else {'missing': True}
out = root/'Saved/QA/ExternalCombatVFX'
out.mkdir(parents=True, exist_ok=True)
(out/'inspection.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log('PGExternalVFX INSPECT PASS')

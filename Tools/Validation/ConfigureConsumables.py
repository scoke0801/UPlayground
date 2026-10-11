"""Back up and connect the healing definition, native Enhanced Input action and feedback."""
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
POTION = '/Game/DataCenter/Consumables/DA_MoonlightPotion'
ACTION = '/Game/Blueprints/Input/Actions/IA_HealingPotion'
CONFIG = '/Game/Blueprints/Actor/LocalPlayer/DA_InputConfig'
CATALOG = '/Game/DataCenter/Progression/DA_PGProgression'
VFX_SOURCE = '/Game/ExternalAssets/VFX/MixedVFX/Particles/Mix/NS_HealingAura'
VFX = '/Game/Art/Consumables/NS_MoonlightHealing'
SOUND = '/Game/Art/Consumables/S_MoonlightHealing'
TAG = 'InputTag.HealingPotion'
VALUES = json.loads((ROOT/'Tools/Validation/Data/Consumables.json').read_text(encoding='utf-8-sig'))

def mappings(context):
    return list(context.get_editor_property('default_key_mappings').get_editor_property('mappings'))

def save(asset):
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()

def sound_source():
    spec=json.loads((ROOT/'Tools/Validation/Data/ExternalSFX.json').read_text(encoding='utf-8'))
    return ROOT/spec['source_directory']/(spec['replacements'][SOUND]+'.wav')

def validate():
    potion, config, catalog, action = [unreal.load_asset(p) for p in (POTION,CONFIG,CATALOG,ACTION)]
    assert all((potion,config,catalog,action))
    assert catalog.get_editor_property('healing_potion') == potion
    for name,value in VALUES.items():
        actual = potion.get_editor_property(name)
        assert abs(float(actual)-value)<.0001 if isinstance(value,float) else str(actual)==str(value), (name,actual,value)
    assert potion.get_editor_property('heal_vfx') and potion.get_editor_property('heal_sound')
    assert action.get_editor_property('value_type') == unreal.InputActionValueType.BOOLEAN
    entries = [x for x in config.get_editor_property('native_input_actions') if str(x.get_editor_property('input_tag').get_editor_property('tag_name'))==TAG]
    assert len(entries)==1 and entries[0].get_editor_property('input_action')==action
    context=config.get_editor_property('default_mapping_context')
    keys=[str(m.get_editor_property('key').get_editor_property('key_name')) for m in mappings(context) if m.get_editor_property('action')==action]
    assert keys==['Q'], keys
    for m in mappings(context):
        if str(m.get_editor_property('key').get_editor_property('key_name'))=='Q':
            assert m.get_editor_property('action')==action, 'Q conflict'
    unreal.log('PGConsumables VALIDATION PASS key=Q heal=.4 capacity=3 cooldown=8')

def apply():
    config, catalog = [unreal.load_asset(p) for p in (CONFIG,CATALOG)]
    assert config and catalog and unreal.load_asset(VFX_SOURCE)
    context = config.get_editor_property('default_mapping_context')
    before = mappings(context)
    for m in before:
        if str(m.get_editor_property('key').get_editor_property('key_name'))=='Q':
            existing=m.get_editor_property('action')
            assert existing and existing.get_path_name().split('.')[0]==ACTION, 'Q already bound: '+str(existing)
    backup=ROOT/'Saved/Backups/Consumables'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup.mkdir(parents=True)
    preserved=[]
    for path in [POTION,ACTION,CONFIG,CATALOG,context.get_path_name(),VFX,SOUND]:
        relative=path.split('.')[0].removeprefix('/Game/')+'.uasset'
        source=ROOT/'Content'/relative
        if source.exists():
            target=backup/relative; target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(source,target); preserved.append((source,target))
    (backup/'manifest.json').write_text(json.dumps([str(s.relative_to(ROOT/'Content')) for s,_ in preserved]),encoding='utf-8')
    unreal.log('PGConsumables BACKUP '+str(backup))
    try:
        potion=unreal.load_asset(POTION)
        if not potion:
            factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.PGConsumableData)
            potion=TOOLS.create_asset('DA_MoonlightPotion','/Game/DataCenter/Consumables',unreal.PGConsumableData,factory)
        for name,value in VALUES.items(): potion.set_editor_property(name,value)
        vfx=unreal.load_asset(VFX) if EAL.does_asset_exist(VFX) else EAL.duplicate_asset(VFX_SOURCE,VFX)
        assert vfx
        task=unreal.AssetImportTask()
        for name,value in dict(filename=str(sound_source()),destination_path='/Game/Art/Consumables',destination_name='S_MoonlightHealing',automated=True,replace_existing=True,save=True).items(): task.set_editor_property(name,value)
        TOOLS.import_asset_tasks([task])
        sound=unreal.load_asset(SOUND); assert sound
        potion.set_editor_property('heal_vfx',vfx); potion.set_editor_property('heal_sound',sound)
        potion.set_editor_property('heal_color',unreal.LinearColor(.12,1.,.7,1.))
        action=unreal.load_asset(ACTION)
        if not action: action=TOOLS.create_asset('IA_HealingPotion','/Game/Blueprints/Input/Actions',unreal.InputAction,unreal.InputAction_Factory())
        action.set_editor_property('value_type',unreal.InputActionValueType.BOOLEAN)
        action.set_editor_property('trigger_when_paused',False)
        entry=unreal.PGInputActionConfig()
        entry.import_text('(InputTag=(TagName="'+TAG+'"),InputAction="'+action.get_path_name()+'")')
        native=[x for x in config.get_editor_property('native_input_actions') if str(x.get_editor_property('input_tag').get_editor_property('tag_name'))!=TAG]
        config.set_editor_property('native_input_actions',native+[entry])
        if not any(m.get_editor_property('action')==action for m in before):
            key=unreal.Key(); key.set_editor_property('key_name','Q'); context.map_key(action,key)
        catalog.set_editor_property('healing_potion',potion)
        for asset in (action,context,config,vfx,potion,catalog): save(asset)
        validate()
        unreal.log('PGConsumables APPLY PASS backup='+str(backup))
    except Exception:
        # Loaded packages may be file-mapped on Windows. The runner restores after this process exits.
        unreal.log_error('PGConsumables failed; runner must restore existing packages from '+str(backup))
        raise

if __name__=='__main__':
    validate() if '-PGConsumablesValidate' in unreal.SystemLibrary.get_command_line() else apply()

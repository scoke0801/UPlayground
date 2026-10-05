"""Apply the reviewed monster kits with per-run backups; preserve unrelated data and art."""
import copy
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir())
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
SPEC = json.loads((ROOT / 'Tools/Validation/CombatVariety.json').read_text(encoding='utf-8'))
OUT = '/Game/DataCenter/CombatVariety'
BACKUP = ROOT / 'Saved/Backups/CombatVariety' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
BACKUP.mkdir(parents=True)
ENEMIES = '/Game/DataCenter/DataTables/Actor/DT_Enemy'
SKILLS = '/Game/DataCenter/DataTables/Skill/DT_Skill'


def preserve(path):
    relative = path.split('.')[0].removeprefix('/Game/') + '.uasset'
    source, target = ROOT / 'Content' / relative, BACKUP / relative
    if source.is_file() and not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))


def material():
    path = OUT + '/M_PGCombatVarietyBounds'
    preserve(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat = unreal.load_asset(path)
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_PGCombatVarietyBounds', OUT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    lib = unreal.MaterialEditingLibrary
    def vector(name, value):
        node = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', unreal.LinearColor(*value))
        return node
    lib.connect_material_property(vector('GradeColor', (1,.18,.025,1)), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    nodes = {'P': lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition),
             'C': vector('Center', (0,0,0,1)), 'F': vector('Forward', (1,0,0,1))}
    for name, value in [('Radius',280),('Length',650),('HalfWidth',70),('Shape',0),('CosAngle',.42),
                        ('InnerRadius',160),('VolleyCount',1),('SpreadRadians',.384)]:
        node = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', value)
        nodes[name] = node
    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    custom.set_editor_property('code', '''float2 d=P.xy-C.xy;
float r=length(d), along=dot(d,F.xy), side=abs(d.x*F.y-d.y*F.x);
bool inside=false; float edge=0;
if (Shape>3.5) {
    float alpha=0; int count=clamp((int)VolleyCount,1,5);
    for(int i=0;i<5;i++) {
        if(i>=count) break;
        float angle=count<=1 ? 0 : lerp(-SpreadRadians,SpreadRadians,(float)i/(count-1));
        float2 f=float2(F.x*cos(angle)-F.y*sin(angle),F.x*sin(angle)+F.y*cos(angle));
        float a=dot(d,f), s=abs(d.x*f.y-d.y*f.x);
        if(a>=0 && a<=Length && s<=HalfWidth) {
            float e=min(min(a,Length-a),HalfWidth-s);
            alpha=max(alpha,e<6 ? 0.95 : 0.2);
        }
    }
    return alpha;
} else if(Shape>2.5) {
    inside=r>=InnerRadius && r<=Radius; edge=min(r-InnerRadius,Radius-r);
} else if(Shape>1.5) {
    inside=along>=0 && along<=Length && side<=HalfWidth;
    edge=min(min(along,Length-along),HalfWidth-side);
} else {
    inside=r<=Radius && (Shape<0.5 || r<0.001 || along/r>=CosAngle);
    edge=min(Radius-r,Shape>0.5 ? (along/max(r,0.001)-CosAngle)*r : Radius);
}
return inside ? (edge<8 ? 0.95 : 0.16) : 0;''')
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for name in nodes:
        item = unreal.CustomInput(); item.set_editor_property('input_name', name); inputs.append(item)
    custom.set_editor_property('inputs', inputs)
    for name, node in nodes.items(): lib.connect_material_expressions(node, '', custom, name)
    lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat.get_path_name()


enemies, skills = rows(ENEMIES), rows(SKILLS)
before = {'enemies': copy.deepcopy(enemies), 'skills': copy.deepcopy(skills)}
(BACKUP / 'before.json').write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding='utf-8')
by_id = {row['SkillID']: row for row in skills}
assert all(sid in by_id for sid in range(15101,15109)), 'Apply content milestones 1 and 2 first'
for row in enemies:
    if str(row['EnemyID']) in SPEC['enemy_kits']:
        assert row['Role'] != 'Legacy', row['EnemyID']
for definition in SPEC['new_skills']:
    sid = definition['SkillID']
    if sid in by_id:
        assert by_id[sid]['Name'] == 'CombatVariety_' + str(sid), 'ID collision: ' + str(sid)
    row = copy.deepcopy(by_id[definition['base']])
    row.update({key:value for key,value in definition.items() if key != 'base'})
    row['Name'] = 'CombatVariety_' + str(sid)
    by_id[sid] = row
for sid, patch in SPEC['existing'].items(): by_id[int(sid)].update(patch)
for row in enemies:
    if str(row['EnemyID']) in SPEC['enemy_kits']:
        row['SkillIdList'] = SPEC['enemy_kits'][str(row['EnemyID'])]
        if row['EnemyID'] == 15106: row['PhaseTwoSkillSequence'] = SPEC['boss_sequence']
bound_material = material()
affected_skills = set(int(sid) for sid in SPEC['existing']) | {r['SkillID'] for r in SPEC['new_skills']}
for sid in affected_skills: by_id[sid]['TelegraphMaterial'] = bound_material
skills = list(by_id.values())
assert [r for r in enemies if str(r['EnemyID']) not in SPEC['enemy_kits']] == [r for r in before['enemies'] if str(r['EnemyID']) not in SPEC['enemy_kits']]
assert [r for r in skills if r['SkillID'] not in affected_skills] == [r for r in before['skills'] if r['SkillID'] not in affected_skills]
for path, data in [(SKILLS,skills),(ENEMIES,enemies)]:
    preserve(path)
    table = unreal.load_asset(path)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(data,ensure_ascii=False))
    assert unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)
from ValidateCombatVariety import validate_combat_variety
from ConfigureSkeletonArcher import apply as configure_skeleton_archer
configure_skeleton_archer()
result = validate_combat_variety({r['EnemyID']:r for r in rows(ENEMIES)}, {r['SkillID']:r for r in rows(SKILLS)})
(BACKUP / 'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2), encoding='utf-8')
unreal.log('PGCombatVariety MIGRATION PASS ' + json.dumps(dict(result,backup=str(BACKUP))))

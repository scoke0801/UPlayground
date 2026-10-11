"""Build the explicit, reviewable P09 roster from the existing motion inventory."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def source(folder, stem):
    files = [p for p in (ROOT/'Content/Art/AnimationTests'/folder).glob(stem+'*.uasset')
             if re.fullmatch(re.escape(stem)+r'[0-9a-z]{10,11}', p.stem)]
    assert len(files) == 1, (folder, stem, files)
    return '/Game/'+files[0].relative_to(ROOT/'Content').with_suffix('').as_posix()


def main():
    families = {}
    for name, pack, mesh, prefix in [('Spear','Spear','FrankSpear','AS_PGFrank_RPG_Spear_'),
                                     ('Greatsword','GreatSword','FrankGreatSword','AS_PGGreatSword_')]:
        folder = 'FrankSlash/Animations/'+pack
        motions = {'Idle': source(folder, prefix+'Equip_Idle_')}
        for mode in ('Walk', 'Run'):
            for direction in ('F','FR','R','BR','B','BL','L','FL'):
                motions[mode+'_'+direction] = source(folder,prefix+'8Way_'+mode+'_'+direction+'_')
        families[name] = dict(source_mesh='/Game/Art/AnimationTests/FrankSlash/Setup/'+mesh+'/SK_PG'+mesh, motions=motions)
    # A staff uses the two-handed pole carry set, with separate RPG spell casts.
    families['Staff'] = dict(carry_family='Spear')
    skills = []
    for index in range(3):
        skills.append(dict(id=15751+index, family='Spear', source=source('FrankSlash/Animations/Spear',f'AS_PGFrank_RPG_Spear_Attack0{2 if index==1 else 1}_'),
            motion_key='15752' if index==1 else '15751',
            source_mesh=families['Spear']['source_mesh'], name=['창병의 회전 창격','창병의 내려 베기','창대장의 축력 창격'][index],
            base=15101, contact=[.20,.335,.20][index],
            patch=dict(Pattern='Sweep',TelegraphDuration=[.8,.95,1.2][index],RecoveryDuration=[.9,1.15,1.5][index],SkillCoolTime=[3,5,7][index],
                SkillRange=280,TravelDistance=280,TelegraphRadius=280,LineHalfWidth=42,HalfAngleDegrees=[65,50,85][index],MinimumActivationRange=0,EnemyDamageMultiplier=[1.,1.1,1.4][index],bHeavyImpactFeedback=index==2)))
        skills.append(dict(id=15754+index, family='Greatsword', source=source('GrruzamSword/Animations/M_Big_Sword',f'AS_PGAttack_3Combo_{index+1}_Inplace_'),
            source_mesh='/Game/Art/AnimationTests/GrruzamSword/Setup/Grruzam/SK_PGGrruzam',name=['대검병의 횡베기','대검병의 올려베기','대검대장의 내려찍기'][index],
            base=15101, contact=[.175,.225,.30][index], patch=dict(Pattern='Sweep',TelegraphDuration=[1.,1.1,1.35][index],RecoveryDuration=[1.2,1.35,1.7][index],SkillCoolTime=[3.,5.,7.][index],
                SkillRange=250,TelegraphRadius=250,HalfAngleDegrees=[75,50,35][index],MinimumActivationRange=0,EnemyDamageMultiplier=[1.25,1.3,1.65][index],bHeavyImpactFeedback=True)))
        skills.append(dict(id=15757+index, family='Staff', source=source('RPGAnimations/Animations/Magic',f'AS_PGMagic_Attack_{index+1}_InPlace_'),
            source_mesh='/Game/Art/AnimationTests/RPGAnimations/Setup/RPG/SK_PGRPG',name=['술사의 마력탄','술사의 봉인 지대','대마도사의 마력 폭발'][index],
            base=15102 if index==0 else 15105 if index==1 else 15114,contact=[.48,.48,.5][index],
            patch=dict(Pattern=['AimedProjectile','HazardSequence','Sweep'][index],HalfAngleDegrees=180,TelegraphDuration=[1.,1.3,1.5][index],RecoveryDuration=[.9,1.3,1.5][index],SkillCoolTime=[3.,6.,8.][index],
                SkillRange=[1000,950,360][index],TravelDistance=1100,TravelSpeed=700,TelegraphRadius=[100,150,360][index],LineHalfWidth=24,HazardCount=3,HazardSpacing=150,
                MinimumActivationRange=0,EnemyDamageMultiplier=[1.,.85,1.3][index],ProjectileClass='/Script/PGActor.PGPatternProjectile',bHeavyImpactFeedback=index==2)))
    monsters = []
    for fidx,(family,title,base,speed,hp,attack) in enumerate([('Spear','창병',15101,350,1250,85),('Greatsword','대검병',15101,305,1550,95),('Staff','술사',15102,315,1000,85)]):
        for grade,armor,mult in [('Normal','003',1.),('Veteran','007',1.5),('Elite','012',2.4)]:
            gidx=['Normal','Veteran','Elite'].index(grade)
            for sex in ('Female','Male'):
                eid=15701+fidx*6+gidx*2+(sex=='Male')
                monsters.append(dict(id=eid,family=family,sex=sex,grade=grade,armor=armor,
                    name=('숙련 ' if gidx==1 else '정예 ' if gidx==2 else '')+title,
                    base=base,skills=list(range(15751+fidx*3,15752+fidx*3+gidx)),
                    stats=dict(Health=round(hp*mult),Attack=round(attack*(1+gidx*.35)),Defense=30+gidx*40,MovementSpeed=speed-gidx*10),
                    drop_pool='Rogue.Crusher' if gidx==2 else 'Rogue.Shooter' if family=='Staff' else 'Rogue.Chaser'))
    replacements=[(1,1,15102,15701),(1,2,15102,15702),(2,1,15103,15707),(2,1,15103,15708),(1,2,15102,15713),(1,3,15102,15714),
                  (3,1,15102,15703),(3,1,15102,15704),(3,2,15203,15709),(3,2,15203,15710),(3,2,15102,15715),(3,2,15102,15716),
                  (4,2,15205,15705),(4,2,15206,15706),(5,1,15205,15711),(4,3,15205,15712),(5,2,15205,15717),(5,2,15206,15718)]
    spec=dict(schema=1,families=families,skills=sorted(skills,key=lambda d:d['id']),monsters=monsters,
              wave_replacements=[dict(stage=s,wave=w,source=src,enemy=eid) for s,w,src,eid in replacements],
              grips={sex:dict(translation=[-.009,-.055 if sex=='Female' else -.06,-.03 if sex=='Female' else -.033],rotation=[0,90,0],scale=[.01,.01,.01]) for sex in ('Female','Male')})
    (ROOT/'Tools/Validation/Data/P09WeaponVariations.json').write_text(json.dumps(spec,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__': main()

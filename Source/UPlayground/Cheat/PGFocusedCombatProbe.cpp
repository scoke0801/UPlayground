#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
#include "PGCombatFilm.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGAI/PGRoleAIController.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataAsset/Progression/PGItemGeneration.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGUI/Manager/PGUIManager.h"

static FAutoConsoleCommandWithWorld PGFocusedCombatProbe(TEXT("PGFocusedCombatProbe"),
    TEXT("Measure a starter Bokusei versus spear infantry in an isolated FocusedCombat_ profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),Profile) || !Profile.StartsWith(TEXT("FocusedCombat_"))) return;
    struct FState
    {
        double Started=FPlatformTime::Seconds();
        float ReadyAt=-1.f, AttackAt=-1.f, LastHealth=0.f, LastPlayerHealth=0.f;
        int32 EnemyAttacks=0, Hits=0;
        bool bNormalStarted=false;
        FString Character=TEXT("Bokusei");
        bool bCritical=FParse::Param(FCommandLine::Get(),TEXT("PGFocusedCritical"));
        bool bEquipment=FParse::Param(FCommandLine::Get(),TEXT("PGFocusedEquipment"));
        bool bTwoPacks=FParse::Param(FCommandLine::Get(),TEXT("PGFocusedTwoPacks"));
        int32 Pack=1;
        float ObserveSeconds=10.f, FirstClear=-1.f;
        bool bFirstHit=false;
        FString LastInputState;
        FVector FirstOrigin=FVector::ZeroVector;
        bool bTransitionDashObserved=false;
        double LastSkillRequestClock=-1.;
        bool bBleed=FParse::Param(FCommandLine::Get(),TEXT("PGFocusedBleed"));
        bool bMixed=FParse::Param(FCommandLine::Get(),TEXT("PGFocusedMixed"));
        bool bLoop=FParse::Param(FCommandLine::Get(),TEXT("PGFocusedLoop"));
        bool bOpeningLogged=false;
        bool bRefundLogged=false;
        FGuid ObservedCast;
        int32 Basics=0;
        bool bSkillFirst=FParse::Param(FCommandLine::Get(),TEXT("PGFocusedSkill"));
        TWeakObjectPtr<APGCharacterEnemy> Enemy;
        TArray<TWeakObjectPtr<APGCharacterEnemy>> Enemies;
        TArray<float> Health;
        TArray<bool> Active, Recovering;
        FPGCombatFilm Film;
    };
    auto S=MakeShared<FState>(); TWeakObjectPtr<UWorld> Weak=World;
    FParse::Value(FCommandLine::Get(),TEXT("PGFocusedCharacter="),S->Character);
    FParse::Value(FCommandLine::Get(),TEXT("PGFocusedObserve="),S->ObserveSeconds);
    if (!FMath::IsFinite(S->ObserveSeconds) || S->ObserveSeconds<0.f || S->ObserveSeconds>10.f) return;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak](float)
    {
        auto* W=Weak.Get(); if (!W) return false;
        const auto Finish=[](bool OK,const TCHAR* Reason)
        { UE_LOG(LogTemp,Display,TEXT("PGFocusedCombatProbe %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Reason); FPlatformMisc::RequestExit(false); return false; };
        if (FPlatformTime::Seconds()-S->Started>600.) return Finish(false,TEXT("timeout"));
        auto* P=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        if (!P || !P->GetSkillHandler() || FPlatformTime::Seconds()-S->Started<5.) return true;
        auto* ASC=P->GetPGAbilitySystemComponent();
        const float Now=W->GetTimeSeconds();
        if (S->ReadyAt<0.f)
        {
            if (auto* UI=UPGUIManager::Get(P)) UI->CloseAllUI();
            if (!P->IsGameplayInputAllowed()) return true;
            auto* Profile=UPGProfileSubsystem::Get(P);
            if (S->Pack==1)
            {
                S->FirstOrigin=P->GetActorLocation();
                if (!Profile->SelectCharacter(FName(*S->Character))) return Finish(false,TEXT("character selection"));
                if (S->bEquipment)
                {
                    if (!Profile->MarkRunAssisted() || !Profile->SelectActiveSkills({112,111,113,114})) return Finish(false,*FString::Printf(TEXT("saved skill selection: %s"),*Profile->Status));
                    if (S->bBleed)
                    {
                        const auto* Catalog=Profile->GetCatalog();
                        const auto* Def=Catalog->FindItem(15004);
                        const auto* Rule=Def ? Catalog->RarityRules.FindByPredicate([&](const auto& R){return R.Rarity==Def->Rarity;}) : nullptr;
                        FPGItemInstance Item; FString Error; FRandomStream Random(173001);
                        if (!Def || !Rule || !PGItemGeneration::Generate(*Def,*Rule,Random,Item,Error)) return Finish(false,TEXT("equipment generation"));
                        Catalog->InitializeLegendary(Item);
                        if (!Profile->TryPickup(Item) || !Profile->Equip(Item.Guid)) return Finish(false,TEXT("equipment transaction"));
                        for (int32 Id : {15003,15018})
                        {
                            const auto* Reward=UPGDataTableManager::Get(W)->GetRowData<FPGRewardStatDataRow>(Id);
                            if (!Reward || !Profile->CommitReward(FGuid::NewGuid(),2,Reward->StatType,Reward->Amount,Reward->Perk,Reward->PerkPercent,Id,false)) return Finish(false,TEXT("reward selection"));
                        }
                        UE_LOG(LogTemp,Display,TEXT("PGDuel Equipment Definition=%d Guid=%s"),Item.DefinitionId,*Item.Guid.ToString());
                    }
                    UE_LOG(LogTemp,Display,TEXT("PGDuel Build Equipment=1 Bleed=%d Burst=%d Spread=%d Recast=%d"),Profile->GetEffectivePerk(EPGCombatPerk::Bleed),Profile->GetEffectivePerk(EPGCombatPerk::BleedBurst),Profile->GetEffectivePerk(EPGCombatPerk::BleedSpread),Profile->GetEffectivePerk(EPGCombatPerk::BleedRecast));
                }
            }
            auto* Aim=FindFProperty<FBoolProperty>(P->GetClass(),TEXT("bUseQuarterView"));
            if (!Aim) return Finish(false,TEXT("fixed aim property"));
            Aim->SetPropertyValue_InContainer(P,false);
            if (S->Pack==1) P->SetActorRotation(FRotator::ZeroRotator);
            const auto* Row=UPGDataTableManager::Get(W)->GetRowData<FPGEnemyDataRow>(15701);
            auto* Class=Row?Row->ActorClass.LoadSynchronous():nullptr;
            if (!Class) return Finish(false,TEXT("missing enemy"));
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            int32 Count=1;
            FParse::Value(FCommandLine::Get(),TEXT("PGFocusedEnemies="),Count);
            if (Count!=1 && Count!=3 && Count!=10) return Finish(false,TEXT("unsupported enemy count"));
            const auto SetFixtureCritical=[](UPGAbilitySystemComponent* Target,float Rate)
            {
                const auto Attribute=UPGAtrributeSet::GetCriticalRateAttribute();
                Target->SetNumericAttributeBase(Attribute,0.f);
                // These fixtures use additive equipment/profile bonuses; preserve those effects.
                const float Bonus=Target->GetCombatStat(EPGStatType::CriticalRate);
                Target->SetNumericAttributeBase(Attribute,Rate-Bonus);
                return FMath::IsNearlyEqual(Target->GetCombatStat(EPGStatType::CriticalRate),Rate);
            };
            for (int32 Index=0; Index<Count; ++Index)
            {
                const int32 EnemyID=S->bMixed && Index==0 ? 15705 : 15701;
                const auto* SpawnRow=UPGDataTableManager::Get(W)->GetRowData<FPGEnemyDataRow>(EnemyID);
                auto* SpawnClass=SpawnRow ? SpawnRow->ActorClass.LoadSynchronous() : nullptr;
                if (!SpawnClass) return Finish(false,TEXT("missing encounter enemy"));
                const FVector Offset=Count==10 ? FVector(180+(Index/3)*110,((Index%3)-1)*110,0) : Index==0 ? FVector(180,0,0) : FVector(200,Index==1?110:-110,0);
                const FVector SpawnLocation=S->Pack==1 ? P->GetActorLocation()+Offset : S->FirstOrigin-FVector(700+Offset.X,Offset.Y,0);
                auto* Spawned=W->SpawnActor<APGCharacterEnemy>(SpawnClass,SpawnLocation,FRotator(0,S->Pack==1?180:0,0),Params);
                if (!Spawned) return Finish(false,TEXT("spawn"));
                Spawned->SpawnDefaultController();
                auto* AI=Cast<APGRoleAIController>(Spawned->GetController());
                if (!AI) return Finish(false,TEXT("missing role AI"));
                AI->SetCombatThinkingEnabled(true);
                if (!SetFixtureCritical(Spawned->GetPGAbilitySystemComponent(),0.f)) return Finish(false,TEXT("enemy effective critical rate"));
                S->Enemies.Add(Spawned); S->Health.Add(Spawned->GetPGAbilitySystemComponent()->GetHealth());
                S->Active.Add(false); S->Recovering.Add(false);
                UE_LOG(LogTemp,Display,TEXT("PGDuel Placement Index=%d Location=%s Health=%.3f Defense=%.3f Critical=0"),
                    Index,*Spawned->GetActorLocation().ToString(),S->Health.Last(),Spawned->GetPGAbilitySystemComponent()->GetCombatStat(EPGStatType::Defense));
            }
            auto* E=S->Enemies[0].Get();
            // Deterministic critical fixture; enemies remain noncritical in both cases.
            if (!SetFixtureCritical(ASC,S->bCritical ? 10000.f : 0.f)) return Finish(false,TEXT("player effective critical rate"));
            if (S->Pack==1 && !S->bEquipment)
            {
                if (S->bBleed) ASC->SetCombatPerks({{EPGCombatPerk::Bleed,10},{EPGCombatPerk::BleedBurst,100},{EPGCombatPerk::BleedSpread,100},{EPGCombatPerk::BleedRecast,1}});
                P->GetSkillHandler()->RemoveSkill(EPGSkillSlot::SkillSlot_1);
                P->GetSkillHandler()->AddSkill(EPGSkillSlot::SkillSlot_1,112);
            }
            S->Enemy=E; S->ReadyAt=Now;
            UE_LOG(LogTemp,Display,TEXT("PGDuel Pack Begin=%d world=%.6f Observe=%.3f"),S->Pack,Now,S->Pack==1?S->ObserveSeconds:0.f);
            UE_LOG(LogTemp,Display,TEXT("PGDuel Conditions Count=%d Mixed=%d Bleed=%d Seed=173001"),Count,S->bMixed,S->bBleed);
            UE_LOG(LogTemp,Display,TEXT("PGDuel Appearance Character=%s PlayerCritical=%.0f"),*S->Character,ASC->GetCombatStat(EPGStatType::CriticalRate));
            S->LastHealth=E->GetPGAbilitySystemComponent()->GetHealth(); S->LastPlayerHealth=ASC->GetHealth();
            UE_LOG(LogTemp,Display,TEXT("PGDuel Setup Enemy=15701 Health=%.3f PlayerHealth=%.3f Attack=%.3f Defense=%.3f"),
                S->LastHealth,S->LastPlayerHealth,ASC->GetCombatStat(EPGStatType::Attack),ASC->GetCombatStat(EPGStatType::Defense));
            return true;
        }
        S->Film.Capture();
        auto* Attack=P->GetPlayerAttackComponent();
        if (S->Pack==2 && !S->bTransitionDashObserved && P->GetPlayerDashComponent()->IsDashing())
        {
            S->bTransitionDashObserved=true;
            UE_LOG(LogTemp,Display,TEXT("PGDuel Transition Dash world=%.6f Facing=%s Location=%s"),Now,*P->GetActorForwardVector().ToString(),*P->GetActorLocation().ToString());
            if (FVector::DotProduct(P->GetActorForwardVector(),-FVector::ForwardVector)<.5f) return Finish(false,TEXT("transition dash facing away from next pack"));
        }
        FGameplayTagContainer Tags; ASC->GetOwnedGameplayTags(Tags);
        const auto* Montage=P->GetMesh()->GetAnimInstance() ? P->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage() : nullptr;
        const FString InputState=FString::Printf(TEXT("Allowed=%d Control=%d HitStop=%d Dash=%d Health=%.3f Montage=%s Tags=%s"),P->IsGameplayInputAllowed(),P->GetIsCacControl(),P->IsAttackHitStopped(),P->GetPlayerDashComponent()->IsDashing(),ASC->GetHealth(),*GetNameSafe(Montage),*Tags.ToStringSimple());
        if (InputState!=S->LastInputState)
        {
            S->LastInputState=InputState;
            UE_LOG(LogTemp,Display,TEXT("PGDuel State world=%.6f %s"),Now,*InputState);
        }
        if (const auto Context=Attack->GetCastContext())
        {
            if (S->ObservedCast!=Context->CastId)
            {
                S->ObservedCast=Context->CastId; S->bOpeningLogged=false; S->bRefundLogged=false;
                UE_LOG(LogTemp,Display,TEXT("PGDuel Cast Pack=%d Skill=%d world=%.6f"),S->Pack,Context->SkillID,Now);
                if (Context->SkillID>=100 && Context->SkillID<=102) ++S->Basics;
            }
            if (Context->bRefundUsed && !S->bRefundLogged)
            {
                S->bRefundLogged=true;
                UE_LOG(LogTemp,Display,TEXT("PGDuel Refund cast=%s Pack=%d world=%.6f"),*Context->CastId.ToString(),S->Pack,Now);
            }
            if (!S->bOpeningLogged && Attack->CanCancel(false))
            {
                S->bOpeningLogged=true;
                UE_LOG(LogTemp,Display,TEXT("PGDuel Opening cast=%s world=%.6f"),*Context->CastId.ToString(),Now);
            }
        }
        float HP=0.f;
        for (int32 Index=0; Index<S->Enemies.Num(); ++Index)
        {
            auto* E=S->Enemies[Index].Get();
            if (!E) { if (S->Health[Index]>0.f) return Finish(false,TEXT("living enemy lost")); else continue; }
            const float Current=E->GetPGAbilitySystemComponent()->GetHealth();
            HP+=Current;
            if (E->bPatternActive && !S->Active[Index])
            {
                ++S->EnemyAttacks;
                UE_LOG(LogTemp,Display,TEXT("PGDuel EnemyAttack Time=%.6f Index=%d"),Now-S->ReadyAt,Index);
            }
            if (E->bPatternRecovering!=S->Recovering[Index])
                UE_LOG(LogTemp,Display,TEXT("PGDuel Recovery Time=%.6f Index=%d Active=%d"),Now-S->ReadyAt,Index,E->bPatternRecovering);
            S->Active[Index]=E->bPatternActive; S->Recovering[Index]=E->bPatternRecovering;
            if (Current<S->Health[Index])
            {
                if (!S->bFirstHit)
                {
                    S->bFirstHit=true;
                    const auto* Skill=P->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1);
                    UE_LOG(LogTemp,Display,TEXT("PGDuel FirstDamage Pack=%d world=%.6f SinceClear=%.6f Cooldown=%.6f"),S->Pack,Now,S->FirstClear>=0?Now-S->FirstClear:0.f,Skill?Skill->GetRemainingCooldown():-1.f);
                }
                ++S->Hits;
                UE_LOG(LogTemp,Display,TEXT("PGDuel Hit Time=%.6f Damage=%.3f Health=%.3f Recovery=%d Index=%d HitStop=%d"),
                    Now-S->AttackAt,S->Health[Index]-Current,Current,E->bPatternRecovering,Index,P->IsAttackHitStopped());
                UE_LOG(LogTemp,Display,TEXT("PGDuel Feedback Index=%d TotalStop=%.6f AnimRate=%.3f PatternActive=%d Suppressed=%d"),
                    Index,E->GetTotalHitStopSeconds(),E->GetMesh()->GlobalAnimRateScale,E->bPatternActive,E->GetSuppressedFeedbackRequests());
            }
            S->Health[Index]=Current;
        }
        if (ASC->GetHealth()<=0.f) return Finish(false,TEXT("player defeated"));
        if (HP<=0.f)
        {
            ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_Skill_Normal);
            const auto* Skill=P->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1);
            UE_LOG(LogTemp,Display,TEXT("PGDuel Result TTK=%.6f Hits=%d EnemyAttacks=%d Taken=%.3f Cooldown=%.6f"),Now-S->AttackAt,S->Hits,S->EnemyAttacks,S->LastPlayerHealth-ASC->GetHealth(),Skill ? Skill->GetRemainingCooldown() : -1.f);
            UE_LOG(LogTemp,Display,TEXT("PGDuel Pack Clear=%d world=%.6f Location=%s"),S->Pack,Now,*P->GetActorLocation().ToString());
            if (S->bTwoPacks && S->Pack==1)
            {
                S->FirstClear=Now; ++S->Pack; S->ReadyAt=-1.f; S->AttackAt=-1.f;
                S->Enemies.Reset(); S->Health.Reset(); S->Active.Reset(); S->Recovering.Reset();
                S->Basics=0; S->Hits=0; S->EnemyAttacks=0; S->bNormalStarted=false;
                S->bFirstHit=false;
                P->SetActorRotation(FRotator(0,180,0));
                P->AddMovementInput(-FVector::ForwardVector);
                ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
                ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_Roll);
                UE_LOG(LogTemp,Display,TEXT("PGDuel Transition RollRequest world=%.6f"),Now);
                return true;
            }
            return Finish(S->ObserveSeconds==0.f || S->Pack>1 || S->EnemyAttacks>=2,TEXT("autonomous_enemy=1 manual_input=0"));
        }
        if (Now-S->ReadyAt>30.f) return Finish(false,TEXT("duel did not finish"));
        if (S->Pack==1 && Now-S->ReadyAt<S->ObserveSeconds) return true;
        if (S->bLoop)
        {
            APGCharacterEnemy* Nearest=nullptr;
            double Distance=MAX_dbl;
            for (const auto& Candidate : S->Enemies)
                if (auto* E=Candidate.Get(); E && E->GetPGAbilitySystemComponent()->GetHealth()>0.f)
                {
                    const double D=FVector::DistSquared2D(P->GetActorLocation(),E->GetActorLocation());
                    if (D<Distance) { Distance=D; Nearest=E; }
                }
            if (Nearest)
            {
                const FVector Direction=(Nearest->GetActorLocation()-P->GetActorLocation()).GetSafeNormal2D();
                if (!P->GetPlayerDashComponent()->IsDashing() && (!Attack->IsRunning() || Attack->CanCancel(false))) P->SetActorRotation(Direction.Rotation());
                if (Distance>FMath::Square(180.f) && !P->IsAttackHitStopped()) P->AddMovementInput(Direction);
                if (S->Pack>1 && S->AttackAt<0.f && (Distance>FMath::Square(250.f) || P->GetPlayerDashComponent()->IsDashing())) return true;
            }
        }
        if (S->AttackAt<0.f)
        {
            S->AttackAt=Now;
            UE_LOG(LogTemp,Display,TEXT("PGDuel Input world=%.6f"),Now);
            const auto* Skill=P->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1);
            UE_LOG(LogTemp,Display,TEXT("PGDuel Entry Pack=%d world=%.6f Cooldown=%.6f Location=%s"),S->Pack,Now,Skill?Skill->GetRemainingCooldown():-1.f,*P->GetActorLocation().ToString());
            P->GetSkillHandler()->ResetCombo();
            if (S->bSkillFirst) S->LastSkillRequestClock=FApp::UseFixedTimeStep()?Now:FPlatformTime::Seconds();
            ASC->OnAbilityInputPressed(S->bSkillFirst ? PGGamePlayTags::InputTag_Skill_Slot1 : PGGamePlayTags::InputTag_Skill_Normal);
            S->bNormalStarted=!S->bSkillFirst;
        }
        else if (!S->bNormalStarted && !P->GetPlayerAttackComponent()->IsRunning())
        {
            const double InputAge=(FApp::UseFixedTimeStep()?Now:FPlatformTime::Seconds())-S->LastSkillRequestClock;
            if (InputAge<=ASC->InputBufferSeconds) return true;
            UE_LOG(LogTemp,Display,TEXT("PGDuel Fallback world=%.6f RequestAge=%.6f"),Now,InputAge);
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Normal);
            S->bNormalStarted=true;
        }
        else if (S->bNormalStarted)
        {
            const auto* Skill=P->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1);
            if (S->bLoop && S->Basics>=3 && Skill && !Skill->IsOnCooldown() && (!Attack->IsRunning() || Attack->CanCancel(false)))
            {
                ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_Skill_Normal);
                S->LastSkillRequestClock=FApp::UseFixedTimeStep()?Now:FPlatformTime::Seconds();
                ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
                S->bNormalStarted=false; S->Basics=0;
            }
            else ASC->OnAbilityInputHeld(PGGamePlayTags::InputTag_Skill_Normal);
        }
        return true;
    }));
}));
#endif

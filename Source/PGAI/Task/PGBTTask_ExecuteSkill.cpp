// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTTask_ExecuteSkill.h"
#include "AIController.h"
#include "PGAI/PGCombatSpatial.h"
#include "PGAI/PGRoleAIController.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

UPGBTTask_ExecuteSkill::UPGBTTask_ExecuteSkill()
{
	NodeName = TEXT("Execute Skill");
	bNotifyTick = false;
    bNotifyTaskFinished = true;
    bCreateNodeInstance = true;
	
	SelectedSkillIDKey.SelectedKeyName = FName("SelectedSkillID");
	TargetActorKey.SelectedKeyName = FName("TargetActor");
	SkillTargetActorKey.SelectedKeyName = FName("SkillTargetActor");
	SummonCountKey.SelectedKeyName = FName("SummonCount");
	
	CachedSkillType = EPGSkillType::None;
}

EBTNodeResult::Type UPGBTTask_ExecuteSkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    ResetExecution(true);
    auto* AI = OwnerComp.GetAIOwner();
    auto* Enemy = AI ? Cast<APGCharacterEnemy>(AI->GetPawn()) : nullptr;
    auto* BB = OwnerComp.GetBlackboardComponent();
    auto* ASC = Enemy ? Enemy->GetPGAbilitySystemComponent() : nullptr;
    if (!Enemy || !BB || !ASC || ASC->GetHealth() <= 0 || Enemy->bPerformingHeavyAttack || Enemy->IsBossTransitioning())
        return EBTNodeResult::Failed;
    const int32 SkillID = BB->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName);
    auto* Tables = UPGDataTableManager::Get(Enemy);
    const auto* Skill = Tables ? Tables->GetSkillDataRowByKey(SkillID) : nullptr;
    auto* Handler = Enemy->GetSkillHandler();
    if (!Skill || !Handler || !Handler->IsSkillReadyByID(SkillID) || Skill->MinimumBossPhase > Enemy->BossPhase)
    {
        BB->SetValueAsInt(SelectedSkillIDKey.SelectedKeyName, 0);
        return EBTNodeResult::Failed;
    }
    // Selection and movement are asynchronous. Recheck the actual firing position before committing.
    if (Skill->SkillType == EPGSkillType::Melee || Skill->SkillType == EPGSkillType::Projectile || Skill->SkillType == EPGSkillType::AreaOfEffect)
    {
        auto* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
        const auto* CharacterTarget = Cast<APGCharacterBase>(Target);
        if (!IsValid(Target) || !Skill->IsInActivationRange(FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation())) ||
            !AI->LineOfSightTo(Target) || (CharacterTarget && CharacterTarget->GetPGAbilitySystemComponent() && CharacterTarget->GetPGAbilitySystemComponent()->GetHealth() <= 0))
        {
            BB->SetValueAsInt(SelectedSkillIDKey.SelectedKeyName, 0);
            return EBTNodeResult::Failed;
        }
    }
    const FGameplayTag AbilityTag = GetAbilityTagFromSkillType(Skill->SkillType);
    if (!AbilityTag.IsValid()) return EBTNodeResult::Failed;
    auto* RoleAI = Cast<APGRoleAIController>(AI);
    FGameplayAbilitySpecHandle Handle;
    if (RoleAI) Handle = RoleAI->GetAttackAbilityHandle();
    else
    {
        TArray<FGameplayAbilitySpec*> Specs;
        ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTag.GetSingleTagContainer(), Specs);
        Specs.RemoveAll([](const FGameplayAbilitySpec* Spec) { return !Spec || Spec->IsActive(); });
        if (!Specs.IsEmpty()) Handle = Specs[FMath::RandRange(0, Specs.Num() - 1)]->Handle;
    }
    if (!Handle.IsValid()) return EBTNodeResult::Failed;
    if (Skill->SkillType == EPGSkillType::Heal)
        BB->SetValueAsObject(SkillTargetActorKey.SelectedKeyName, SelectBestHealTarget(Enemy, BB));
    CachedOwnerComp = &OwnerComp;
    CachedASC = ASC;
    CachedSkillType = Skill->SkillType;
    ActiveAbilityHandle = Handle;
    AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::OnAbilityEnded);
    bActivating = true;
    bool bActivated = false;
    {
        const TGuardValue<int32> Request(Enemy->RequestedSkillID, SkillID);
        if (RoleAI) bActivated = RoleAI->TryExecuteSkill(SkillID);
        else
        {
            // Preserve legacy routing, without keeping a spec pointer across activation callbacks.
            auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
            const bool bHadTag = Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(AbilityTag);
            if (Spec) Spec->GetDynamicSpecSourceTags().AddTag(AbilityTag);
            bActivated = ASC->TryActivateAbility(Handle);
            Spec = ASC->FindAbilitySpecFromHandle(Handle);
            if (Spec && !bHadTag) Spec->GetDynamicSpecSourceTags().RemoveTag(AbilityTag);
        }
    }
    bActivating = false;
    bOwnsAttackReservation = bActivated && RoleAI;
    if (!bActivated || bEndedDuringActivation)
    {
        const auto Result = bActivated && bEndedDuringActivation && !bEndedCancelled ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
        FinishTask(&OwnerComp, Result);
        return Result;
    }
    return EBTNodeResult::InProgress;
}

void UPGBTTask_ExecuteSkill::OnAbilityEnded(const FAbilityEndedData& Data)
{
    if (!ActiveAbilityHandle.IsValid() || Data.AbilitySpecHandle != ActiveAbilityHandle) return;
    if (bActivating)
    {
        bEndedDuringActivation = true;
        bEndedCancelled = Data.bWasCancelled;
        return; // ExecuteTask has not returned InProgress yet.
    }
    auto* OwnerComp = CachedOwnerComp.Get();
    const auto Result = Data.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
    FinishTask(OwnerComp, Result);
    if (OwnerComp) FinishLatentTask(*OwnerComp, Result);
}

void UPGBTTask_ExecuteSkill::ResetExecution(bool bCancelAbility)
{
    auto* ASC = CachedASC.Get();
    auto* RoleAI = CachedOwnerComp.IsValid() ? Cast<APGRoleAIController>(CachedOwnerComp->GetAIOwner()) : nullptr;
    const bool bRelease = bOwnsAttackReservation;
    const auto Handle = ActiveAbilityHandle;
    if (ASC) ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
    AbilityEndedHandle.Reset();
    ActiveAbilityHandle = FGameplayAbilitySpecHandle();
    CachedASC.Reset();
    CachedOwnerComp.Reset();
    CachedSkillType = EPGSkillType::None;
    bActivating = bEndedDuringActivation = bEndedCancelled = false;
    bOwnsAttackReservation = false;
    // Remove the delegate before cancelling: cancellation may synchronously end the ability.
    if (bCancelAbility && ASC && Handle.IsValid()) ASC->CancelAbilityHandle(Handle);
    if (bRelease && RoleAI) RoleAI->ReleaseAttackReservation();
}

EBTNodeResult::Type UPGBTTask_ExecuteSkill::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    ResetExecution(true);
    FinishTask(&OwnerComp, EBTNodeResult::Aborted);
    return EBTNodeResult::Aborted;
}

void UPGBTTask_ExecuteSkill::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    ResetExecution(true);
    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UPGBTTask_ExecuteSkill::OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp)
{
    ResetExecution(true);
    Super::OnInstanceDestroyed(OwnerComp);
}

void UPGBTTask_ExecuteSkill::FinishTask(UBehaviorTreeComponent* OwnerComp, EBTNodeResult::Type Result)
{
    if (OwnerComp)
    {
        if (auto* BB = OwnerComp->GetBlackboardComponent())
        {
            if (Result == EBTNodeResult::Succeeded)
            {
                if (CachedSkillType == EPGSkillType::SummonEnemy)
                    BB->SetValueAsInt(SummonCountKey.SelectedKeyName, BB->GetValueAsInt(SummonCountKey.SelectedKeyName) + 1);
                if (!StrafeKey.SelectedKeyName.IsNone())
                    BB->SetValueAsBool(StrafeKey.SelectedKeyName, CheckExecuteStrafe(CachedSkillType));
            }
            BB->SetValueAsInt(SelectedSkillIDKey.SelectedKeyName, 0);
            BB->ClearValue(SkillTargetActorKey.SelectedKeyName);
        }
    }
    ResetExecution(false);
}

AActor* UPGBTTask_ExecuteSkill::SelectBestHealTarget(APGCharacterEnemy* Self, UBlackboardComponent* BlackboardComp) const
{
    return PGCombatSpatial::FindInjuredAlly(Self, AllySearchRadius, 1.f, true);
}

FGameplayTag UPGBTTask_ExecuteSkill::GetAbilityTagFromSkillType(EPGSkillType SkillType) const
{
	switch (SkillType)
	{
	case EPGSkillType::Melee:
		return PGGamePlayTags::Enemy_Ability_MeleeSkill;
		
	case EPGSkillType::Projectile:
		return PGGamePlayTags::Enemy_Ability_ProjectileSkill;
		
	case EPGSkillType::AreaOfEffect:
		return PGGamePlayTags::Enemy_Ability_AOESkill;

	case EPGSkillType::Heal:
		return PGGamePlayTags::Enemy_Ability_HealSkill;
		
	case EPGSkillType::SummonEnemy:
		return PGGamePlayTags::Enemy_Ability_SummonSkill;
		
	default:
		return FGameplayTag::EmptyTag;
	}
}

bool UPGBTTask_ExecuteSkill::CheckExecuteStrafe(EPGSkillType SkillType)
{
	switch (SkillType)
	{
	case EPGSkillType::Melee:
		return FMath::FRand() <= 0.3f;
		
	case EPGSkillType::Projectile:
	case EPGSkillType::AreaOfEffect:
		return FMath::FRand() <= 0.75f;
	
	case EPGSkillType::Heal:
		return FMath::FRand() <= 0.7f;
		
	case EPGSkillType::SummonEnemy:
		return FMath::FRand() <= 0.5f;
		
	default:
		break;
	}
	
	return false;
	
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemyAbilityAttack.h"

#include "AbilitySystemComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

UPGEnemyAbilityAttack::UPGEnemyAbilityAttack() { InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; }

void UPGEnemyAbilityAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 스킬 결정해서 대상 스킬 Ability를 활성화할 수 있어야한다.
	APGCharacterEnemy* Character = GetEnemyCharacterFromActorInfo();
	if (nullptr == Character || Character->IsBossTransitioning() || Character->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        !Character->EnsureCombatReady(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		EndAbilitySelf();
		return;
	}
	if (false == CheckMontageIsPlaying(Character, 0.2f))
	{
		EndAbilitySelf();
		return;
	}
	
	
	FPGEnemySkillHandler* SkillHandler = static_cast<FPGEnemySkillHandler*>(Character->GetSkillHandler());
	if (nullptr == SkillHandler)
	{
		EndAbilitySelf();
		return;
	}

    const FGameplayAbilitySpec* Spec = GetAbilitySystemComponentFromActorInfo()->FindAbilitySpecFromHandle(Handle);
    EPGSkillSlot SelectedSkillSlot = EPGSkillSlot::NormalAttack;
    if (!SkillHandler->ResolveSkillSlot(Character->RequestedSkillID,
        Spec ? Spec->GetDynamicSpecSourceTags() : FGameplayTagContainer(), SelectedSkillSlot))
    { EndAbilitySelf(); return; }

    auto* Tables = UPGDataTableManager::Get(this);
	FPGSkillDataRow* Row = Tables ? Tables->GetRowData<FPGSkillDataRow>(SkillHandler->GetSkillID(SelectedSkillSlot)) : nullptr;
	if(nullptr == Row || Row->MinimumBossPhase > Character->BossPhase)
	{
		EndAbilitySelf();
		return;
	}
    if (GetWorld()->GetTimeSeconds() < Character->NextCombatActionAt ||
        (Row->EnemyProfile.LoadSynchronous() && Row->EnemyProfile.Get()->bGuardCounter &&
         Character->CompletedAttackPatterns < Row->EnemyProfile.Get()->AttacksBeforeGuard)) { EndAbilitySelf(); return; }

    if (Row->TelegraphDuration > 0.f)
    {
        const FPGSkillDataRow Pattern = *Row;
        if (!Pattern.IsPatternValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("PGPattern rejected invalid data skill=%d"), Pattern.SkillID);
            EndAbilitySelf(); return;
        }
        if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbilitySelf(); return; }
        SkillHandler->UseSkill(SelectedSkillSlot);
        BeginElitePattern(Pattern);
        return;
    }
	UAnimMontage* MontageToPlay = nullptr;
	if (UObject* LoadedObject = Row->MontagePath.TryLoad())
	{
		MontageToPlay = Cast<UAnimMontage>(LoadedObject);
	}
	if (nullptr == MontageToPlay)
	{
		EndAbilitySelf();
        return;
	}

	if (UAbilityTask_PlayMontageAndWait* MontageTask = PlayMontageWait(MontageToPlay))
	{
		MontageTask->ReadyForActivation();
	}
	
	SkillHandler->UseSkill(SelectedSkillSlot);
}

void UPGEnemyAbilityAttack::OnGameplayEventReceived(FGameplayEventData Payload)
{
	if (const APGCharacterBase* TargetActor = Cast<APGCharacterBase>(Payload.Target.Get()))
	{
		if (UAbilitySystemComponent* ASC = TargetActor->GetAbilitySystemComponent())
		{
			FGameplayEventData Data;
			Data.Instigator = Payload.Instigator;
			Data.Target = Payload.Target;
			
			ASC->HandleGameplayEvent(PGGamePlayTags::Shared_Event_HitReact, &Data);
		}
	}
}

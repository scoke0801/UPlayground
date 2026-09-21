// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityPlayerSkill.h"
#include "PGSkillActivation.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilityPlayerSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    APGCharacterBase* Character = GetCharacter();
    if (!Character || !Character->GetSkillHandler() || !PGData()) { EndAbilitySelf(); return; }
    FPGSkillHandler* Handler = Character->GetSkillHandler();
    const FPGSkillDataRow* Row = PGData()->GetRowData<FPGSkillDataRow>(Handler->GetSkillID(SlotIndex));
    if (!Row || !Handler->IsCanUseSkill(SlotIndex)) { EndAbilitySelf(); return; }
    const FPGSkillDataRow Data = *Row;
    UAnimMontage* Montage = Cast<UAnimMontage>(Data.MontagePath.TryLoad());
    if (!Montage || !Character->GetMesh()->GetAnimInstance()) { EndAbilitySelf(); return; }
    UAbilityTask_PlayMontageAndWait* Task = PlayMontageWait(Montage);
    if (!Task) { EndAbilitySelf(); return; }
    if (APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(Character))
    {
        Player->FaceAimDirection();
        Player->SetSkillCancelPolicy(Data.AttackCancelRemainingFraction, Data.DodgeCancelRemainingFraction);
    }
    UAbilityTask_WaitGameplayEvent* Wait = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, PGGamePlayTags::Shared_Event_Hit);
    if (Wait) { Wait->EventReceived.AddDynamic(this, &ThisClass::OnGameplayEventReceived); Wait->ReadyForActivation(); }
    Task->ReadyForActivation();
    if (!IsActive()) return;
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbilitySelf(); return; }
    Handler->UseSkill(SlotIndex);
}

void UPGAbilityPlayerSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPGAbilityPlayerSkill::OnGameplayEventReceived(FGameplayEventData Payload)
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

bool UPGAbilityPlayerSkill::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && PGSkillActivation::IsReady(ActorInfo, SlotIndex);
}
UPGAbilityPlayerSkill::UPGAbilityPlayerSkill()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = true;
}
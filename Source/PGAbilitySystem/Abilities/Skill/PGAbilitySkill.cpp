// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilitySkill.h"
#include "PGSkillActivation.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

void UPGAbilitySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    APGCharacterBase* Character = GetCharacter();
    if (!Character || !Character->GetSkillHandler() || !PGData()) { EndAbilitySelf(); return; }
    FPGSkillHandler* Handler = Character->GetSkillHandler();
    const auto* Row = PGData()->GetRowData<FPGSkillDataRow>(Handler->GetSkillID(SlotIndex));
    if (!Row || !Handler->IsCanUseSkill(SlotIndex)) { EndAbilitySelf(); return; }
    const FPGSkillDataRow Data = *Row;
    UAnimMontage* Montage = Cast<UAnimMontage>(Data.MontagePath.TryLoad());
    if (!Montage || !Character->GetMesh()->GetAnimInstance()) { EndAbilitySelf(); return; }
    UAbilityTask_PlayMontageAndWait* Task = PlayMontageWait(Montage);
    if (!Task) { EndAbilitySelf(); return; }
    if (APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(Character))
        Player->SetSkillCancelPolicy(Data.AttackCancelRemainingFraction, Data.DodgeCancelRemainingFraction);
    Task->ReadyForActivation();
    if (!IsActive()) return;
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbilitySelf(); return; }
    Handler->UseSkill(SlotIndex);
}
bool UPGAbilitySkill::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && PGSkillActivation::IsReady(ActorInfo, SlotIndex);
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityPlayerSkill.h"
#include "PGSkillActivation.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"

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
    bUsingPlayerProfile = !Data.PlayerProfile.IsNull();
    if (bUsingPlayerProfile)
    {
        auto* Player = Cast<APGCharacterPlayer>(Character);
        auto* Profile = Data.PlayerProfile.LoadSynchronous();
        auto* Montage = Cast<UAnimMontage>(Data.MontagePath.TryLoad());
        FString Error;
        if (!Player || !Profile || !Profile->Validate(Data.SkillID, Error) ||
            !Player->GetPlayerAttackComponent()->CanPrepare(Profile, Montage, Error))
        {
            UE_LOG(LogTemp, Warning, TEXT("PGSkill rejected Skill=%d Profile=%s Reason=%s"),
                Data.SkillID, *Data.PlayerProfile.ToString(), *Error);
            EndAbilitySelf(); return;
        }
        AttackPlayRate = 1.f;
        auto* Task = PlayMontageWait(Montage);
        if (!Task || !CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbilitySelf(); return; }
        // No hit listener and no collision authority until after Commit.
        Task->ReadyForActivation();
        if (!IsActive()) return;
        if (!Player->GetPlayerAttackComponent()->Start(Profile, Montage, SlotIndex != EPGSkillSlot::NormalAttack,
            FPGPlayerAttackEnded::CreateWeakLambda(this, [this](bool bCancelled)
            { if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled); })))
        { EndAbilitySelf(); return; }
        Observation = Player->GetPGAbilitySystemComponent()->BeginSkillObservation(Data.SkillID,
            Player->GetPlayerAttackComponent()->GetCastContext());
        Handler->UseSkill(SlotIndex);
        return;
    }
    if (!FMath::IsFinite(Data.PlayerAttackPlayRate) || Data.PlayerAttackPlayRate < .5f || Data.PlayerAttackPlayRate > 2.f ||
        !FMath::IsFinite(Data.PlayerMeleeDamageMultiplier) || Data.PlayerMeleeDamageMultiplier < .1f || Data.PlayerMeleeDamageMultiplier > 5.f)
    { EndAbilitySelf(); return; }
    AttackPlayRate = Data.PlayerAttackPlayRate;
    MeleeDamageMultiplier = Data.PlayerMeleeDamageMultiplier;
    bHeavyImpact = Data.bPlayerHeavyImpact;
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
    Observation = Character->GetPGAbilitySystemComponent()->BeginSkillObservation(Data.SkillID);
    if (auto* Player = Cast<APGCharacterPlayer>(Character)) Player->SetAttackAimTracking(true);
    Handler->UseSkill(SlotIndex);
    if (auto* ASC = Character->GetPGAbilitySystemComponent())
        ASC->BeginCombatSkill(SlotIndex);
}

void UPGAbilityPlayerSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
    if (auto* ASC = GetPGAbilitySystemComponentFromActorInfo()) ASC->EndSkillObservation(Observation, bWasCancelled);
    Observation.Reset();
    if (bUsingPlayerProfile)
        if (auto* Player = Cast<APGCharacterPlayer>(GetCharacter())) Player->GetPlayerAttackComponent()->Stop();
    if (auto* Combat = GetCombatComponentFromActorInfo())
        if (Combat->GetCharacterCurrentEquippedWeapon())
            Combat->ToggleWeaponCollision(false, EPGToggleDamageType::CurrentEquippedWeapon);
    if (auto* Player = Cast<APGCharacterPlayer>(GetCharacter())) Player->SetAttackAimTracking(false);
	if (auto* Character = GetCharacter()) if (auto* ASC = Character->GetPGAbilitySystemComponent()) ASC->SetHeavySkill(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPGAbilityPlayerSkill::OnGameplayEventReceived(FGameplayEventData Payload)
{
    if (!IsActive() || bUsingPlayerProfile || Payload.Instigator != GetCharacter()) return;
    if (auto* ASC = GetPGAbilitySystemComponentFromActorInfo())
        ASC->ApplyPlayerMeleeHit(const_cast<APGCharacterBase*>(Cast<APGCharacterBase>(Payload.Target.Get())), MeleeDamageMultiplier, bHeavyImpact);
}

UAbilityTask_PlayMontageAndWait* UPGAbilityPlayerSkill::PlayMontageWait(UAnimMontage* MontageToPlay)
{
    const auto* ASC = GetPGAbilitySystemComponentFromActorInfo();
    auto* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, MontageToPlay, AttackPlayRate * (ASC ? ASC->GetFrenzyRate() : 1.f));
    if (!Task) return nullptr;
    Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
    Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
    if (!bUsingPlayerProfile)
    {
        Task->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
        Task->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
    }
    return Task;
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

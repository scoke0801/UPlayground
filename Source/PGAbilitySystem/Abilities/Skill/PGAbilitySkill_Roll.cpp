#include "PGAbilitySkill_Roll.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

// Preserve the reflected class/slot identity for startup abilities and saves.
UPGAbilitySkill_Roll::UPGAbilitySkill_Roll()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = false;
}

bool UPGAbilitySkill_Roll::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* Info, const FGameplayTagContainer* Source,
    const FGameplayTagContainer* Target, FGameplayTagContainer* Relevant) const
{
    const auto* Player = Info ? Cast<APGCharacterPlayer>(Info->AvatarActor.Get()) : nullptr;
    if (!Player || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
    const auto* Dash = Player->GetPlayerDashComponent();
    return FMath::IsFinite(Dash->Duration) && Dash->Duration >= .15f && Dash->Duration <= 1.f &&
        FMath::IsFinite(Dash->Distance) && Dash->Distance >= 50.f && Dash->Distance <= 1000.f &&
        Super::CanActivateAbility(Handle, Info, Source, Target, Relevant);
}

void UPGAbilitySkill_Roll::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation,
    const FGameplayEventData* Event)
{
    // Dash owns movement/lifetime instead of the old base skill's root-motion roll.
    UPGGameplayAbility::ActivateAbility(Handle, Info, Activation, Event);
    auto* Player = Cast<APGCharacterPlayer>(GetCharacter());
    auto* Handler = Player ? Player->GetSkillHandler() : nullptr;
    const auto* Row = Handler && PGData() ? PGData()->GetRowData<FPGSkillDataRow>(Handler->GetSkillID(SlotIndex)) : nullptr;
    DashMontage = Row ? Cast<UAnimMontage>(Row->MontagePath.TryLoad()) : nullptr;
    if (!Row || !DashMontage || !Player->GetMesh()->GetAnimInstance()) { EndAbilitySelf(); return; }
    auto* Dash = Player->GetPlayerDashComponent();
    auto* Movement = Player->GetCharacterMovement();
    const FVector Direction = Player->GetDodgeDirection().GetSafeNormal2D();
    if (Direction.IsNearlyZero() || !CommitAbility(Handle, Info, Activation)) { EndAbilitySelf(); return; }
    Player->ResetAttackHitStop();
    Player->SetActorRotation(Direction.Rotation());
    Player->SetSkillCancelPolicy(0.f, 0.f);
    bSavedLedgePolicy = Movement->bCanWalkOffLedges;
    bMovementOwned = true;
    Movement->bCanWalkOffLedges = false;
    Movement->StopMovementImmediately();
    Dash->Start();
    Handler->UseSkill(SlotIndex);
    Player->GetPGAbilitySystemComponent()->OnDodgeCommitted();
    auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, DashMontage, DashMontage->GetPlayLength() / Dash->Duration,
        NAME_None, true);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
    MontageTask->ReadyForActivation();
    if (!IsActive()) return;
    auto* MoveTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
        this, TEXT("PlayerDash"), Direction, Dash->Distance / Dash->Duration, Dash->Duration,
        false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, true);
    MoveTask->OnFinish.AddDynamic(this, &ThisClass::OnMontageCompleted);
    MoveTask->ReadyForActivation();
}

void UPGAbilitySkill_Roll::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation,
    bool bReplicate, bool bCancelled)
{
    if (bMovementOwned)
    {
        bMovementOwned = false;
        if (auto* Player = Cast<APGCharacterPlayer>(GetCharacter()))
        {
            Player->GetCharacterMovement()->bCanWalkOffLedges = bSavedLedgePolicy;
            Player->GetPlayerDashComponent()->Stop(bCancelled);
        }
    }
    // Removes the movement source on every completion, death and interruption.
    Super::EndAbility(Handle, Info, Activation, bReplicate, bCancelled);
    DashMontage = nullptr;
}

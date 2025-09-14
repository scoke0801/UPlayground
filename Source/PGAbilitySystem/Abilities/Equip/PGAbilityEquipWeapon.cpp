// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityEquipWeapon.h"

#include "EnhancedInputSubsystems.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Weapon/PGPlayerWeapon.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilityEquipWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay);
	if (nullptr == MontageTask)
	{
		EndAbilitySelf();
		return;
	}

	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCompleted);

	UAbilityTask_WaitGameplayEvent* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,PGGamePlayTags::Player_Event_Equip_Weapon);
	if (nullptr == WaitTask)
	{
		EndAbilitySelf();
		return;
	}

	WaitTask->EventReceived.AddDynamic(this, &ThisClass::OnEventReceived);

	MontageTask->ReadyForActivation();
	WaitTask->ReadyForActivation();
}

void UPGAbilityEquipWeapon::HandleEquipWeapon(APGWeaponBase* Weapon)
{
	if (nullptr == Weapon)
	{
		return;
	}
	APGPlayerWeapon* PlayerWeapon = Cast<APGPlayerWeapon>(Weapon);

	// Add Mapping Context
	if (APGPlayerController* PlayerController = GetPlayerControllerFromActorInfo())
	{
		UEnhancedInputLocalPlayerSubsystem* InputSubSystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
		if (nullptr == InputSubSystem)
		{
			return;
		}

		if (PlayerWeapon)
		{
			if (UInputMappingContext* Context = PlayerWeapon->WeaponData.WeaponInputMappingContext)
			{
				InputSubSystem->AddMappingContext(Context, 1);	
			}
		}
	}
	
	// Add Ability Of Weapon, To Owner
	if (UPGAbilitySystemComponent* AbilitySystemComponent = GetPGAbilitySystemComponentFromActorInfo())
	{
		if (PlayerWeapon)
		{
			TArray<FGameplayAbilitySpecHandle> GrantedAbilitySpecHandles;
			constexpr int32 ApplyLevel = 1;
			
			AbilitySystemComponent->GrantPlayerWeaponAbilities(PlayerWeapon->WeaponData.DefaultWeaponAbilities,
				ApplyLevel, GrantedAbilitySpecHandles);

			Weapon->AssignGrantAbilitySpecHandles(GrantedAbilitySpecHandles);
		}
	}
}

void UPGAbilityEquipWeapon::OnEventReceived(FGameplayEventData Payload)
{
	USkeletalMeshComponent* Mesh = GetOwningComponentFromActorInfo();
	APGCharacterBase* Parent = Cast<APGCharacterBase>(GetOwningActorFromActorInfo());
	if (nullptr == Parent)
	{
		return;
	}

	UPGPawnCombatComponent* CombatComponent = Parent->GetCombatComponent();
	if (nullptr == CombatComponent)
	{
		return;
	}
	APGWeaponBase* Weapon = CombatComponent->GetCharacterCarriedWeaponByTag(WeaponTag);
	if (nullptr == Weapon)
	{
		return;
	}
	
	FAttachmentTransformRules Rule(LocationRule, RotationRule, ScaleRule, true);
	if (Weapon->AttachToComponent(Mesh, Rule, AttachSocketName))
	{
		HandleEquipWeapon(Weapon);
		CombatComponent->SetCurrentEquippedWeaponTag(WeaponTag);
	}
}

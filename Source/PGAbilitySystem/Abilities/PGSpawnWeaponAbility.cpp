// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSpawnWeaponAbility.h"

#include "Abilities/Tasks/AbilityTask_SpawnActor.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"

void UPGSpawnWeaponAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// 월드 가져오기
	UWorld* World = GetWorld();

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 무기 스폰
	APGWeaponBase* SpawnedWeapon = World->SpawnActor<APGWeaponBase>(
		WeaponClassToSpawn,
		FTransform::Identity,
		SpawnParams
	);

	if (nullptr == SpawnedWeapon)
	{
		EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, true);
		return;
	}
	
	// SkeletalMeshComponent 가져오기 (부착할 부모 컴포넌트)
	
	if (USkeletalMeshComponent* OwningComponent = GetOwningComponentFromActorInfo())
	{
		// 무기를 지정된 소켓에 부착
		FAttachmentTransformRules Rule(EAttachmentRule::SnapToTarget, 
EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, true);
		SpawnedWeapon->AttachToComponent(OwningComponent,Rule,SocketNameToAttachTo);
	}

	if (UPGPawnCombatComponent* CombatComponent = GetCombatComponentFromActorInfo())
	{
		// 스폰된 무기를 전투 컴포넌트에 등록
		CombatComponent->RegisterSpawnedWeapon(
			WeaponTagToRegister,
			SpawnedWeapon,
			RegisterAsEquippedWeapon
		);
	}
}

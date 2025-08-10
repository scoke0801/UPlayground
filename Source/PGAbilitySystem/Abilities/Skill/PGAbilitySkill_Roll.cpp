// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilitySkill_Roll.h"

#include "MotionWarpingComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PGActor/Characters/PGCharacterBase.h"

void UPGAbilitySkill_Roll::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                           const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                           const FGameplayEventData* TriggerEventData)
{
	//Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedCharacter = Cast<APGCharacterBase>(GetOwningActorFromActorInfo());
	if (nullptr == CachedCharacter)
	{
		EndAbilitySelf();
		return;
	}
	
	CachedMotionWarpingComponent = CachedCharacter->GetComponentByClass<UMotionWarpingComponent>();
	if (nullptr == CachedMotionWarpingComponent)
	{
		EndAbilitySelf();
		return;
	}

	if (false == ComputeRollDirection())
	{
		EndAbilitySelf();
		return;
	}

	if (false == ComputeRollDistance())
	{
		EndAbilitySelf();
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UPGAbilitySkill_Roll::ComputeRollDirection()
{
	APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwningActorFromActorInfo());
	if (nullptr == Character)
	{
		return false;
	}

	RollingDirection = Character->GetLastMovementInputVector();
	RollingDirection.Normalize(0.0001);

	FRotator TargetRot = UKismetMathLibrary::MakeRotFromX(RollingDirection);
	CachedMotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		RollingDirectionName, Character->GetActorLocation(), TargetRot);

	return true;
}

bool UPGAbilitySkill_Roll::ComputeRollDistance()
{
	int32 Level = GetAbilityLevel();
	
	float Distance = RollingDistanceScalableFloat.GetValueAtLevel(Level);

	FVector RollingOffset = RollingDirection * Distance;
	FVector StartLocation = GetOwningActorFromActorInfo()->GetActorLocation() + RollingOffset;

	FVector ActorUpVector = GetOwningActorFromActorInfo()->GetActorUpVector();
	FVector DownVector = ActorUpVector * -1.0f;  // 위쪽 벡터를 뒤집어서 아래쪽으로
	FVector DownOffset = DownVector * 500.0f;    // 500 유닛 아래로

	FVector EndLocation = StartLocation + DownOffset;
	
	const TArray<AActor*> ActorsToIgnore;
	FHitResult OutHit;
	bool Result = UKismetSystemLibrary::LineTraceSingleForObjects(GetOwningActorFromActorInfo(),
		StartLocation, EndLocation,ObjectTypes, false, ActorsToIgnore,EDrawDebugTrace::Type::None, OutHit, true);

	if (false == Result)
	{
		return false;
	}

	CachedMotionWarpingComponent->AddOrUpdateWarpTargetFromLocation(
		RollTargetLocationName, OutHit.ImpactPoint);

	return true;
}

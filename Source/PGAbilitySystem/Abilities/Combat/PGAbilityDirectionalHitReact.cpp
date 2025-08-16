// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityDirectionalHitReact.h"

#include "Kismet/KismetMathLibrary.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"

void UPGAbilityDirectionalHitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                    const FGameplayEventData* TriggerEventData)
{
	ComputeHitReactDirection(TriggerEventData->Instigator);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

UAnimMontage* UPGAbilityDirectionalHitReact::GetMontageToPlay() const
{
	if (true == MontagePaths.IsValidIndex(CachedMontageIndex))
	{
		if (UObject* LoadedObject = MontagePaths[CachedMontageIndex].TryLoad())
		{
			return Cast<UAnimMontage>(LoadedObject);
		}
	}
	return nullptr;
}

void UPGAbilityDirectionalHitReact::ComputeHitReactDirection(const AActor* Attacker)
{
	const AActor* Owner = GetOwningActorFromActorInfo();
	const FVector VictimForVector = Owner->GetActorForwardVector();
	const FVector VictimToAttackerNormalized = (Attacker->GetActorLocation()
		- Owner->GetActorLocation()).GetSafeNormal();

	const float DotResult = FVector::DotProduct(VictimForVector, VictimToAttackerNormalized);

	float ReactDirection = UKismetMathLibrary::DegAcos(DotResult);

	// 왼손 좌표계
	// 두번째 벡터가 첫번째 벡터 우측에 있으면, 아래 방향
	// 좌측에 있으면 위 방향
	const FVector CrossResult = FVector::CrossProduct(VictimForVector, VictimToAttackerNormalized);
	if (CrossResult.Z < 0.f)
	{
		// 아래방향을 가르키고 있다면, 방향을 반대로
		ReactDirection *= -1.f;
	}

	if (ReactDirection >= -45.0f && ReactDirection <= 45.0f)
	{
		CachedMontageIndex = 0;
	}
	else if (ReactDirection < -45.0f && ReactDirection >= -135.f)
	{
		CachedMontageIndex = 1;
	}
	else if (ReactDirection < -135.0f || ReactDirection > 135.f)
	{
		CachedMontageIndex = 2;
	}
	else if (ReactDirection > 45.0f && ReactDirection<= 135.0f)
	{
		CachedMontageIndex = 3;
	}
}

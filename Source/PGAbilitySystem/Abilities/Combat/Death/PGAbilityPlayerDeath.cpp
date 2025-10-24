// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityPlayerDeath.h"

#include "Components/CapsuleComponent.h"

void UPGAbilityPlayerDeath::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                       bool bReplicateEndAbility, bool bWasCancelled)
{
	// 스켈레탈 메시 컴포넌트의 애니메이션 일시정지
	if (USkeletalMeshComponent* MeshComp = GetOwningComponentFromActorInfo())
	{
		MeshComp->bPauseAnims = true;
	}
	
	// 플레이어 캐릭터의 충돌 비활성화
	if (AActor* PlayerCharacter = Cast<AActor>(GetOwningActorFromActorInfo()))
	{
		if (UCapsuleComponent* Capsule = PlayerCharacter->GetComponentByClass<UCapsuleComponent>())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

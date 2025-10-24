// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityHitReact.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/KismetMathLibrary.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Stat/PGStatComponent.h"

void UPGAbilityHitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                         const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (const APGCharacterBase* Instigator = Cast<APGCharacterBase>(TriggerEventData->Instigator))
	{
		if (APGCharacterBase* Owner = GetCharacter())
		{
			// 사망한 대상에게는 추가 처리 진행하지 않는다.
			if (UPGStatComponent* StatComponent = Owner->GetStatComponent())
			{
				if (FMath::IsNearlyZero(StatComponent->CurrentHealth, 0.01f))
				{
					EndAbilitySelf();
					return;
				}
			}
			Owner->OnHit(Instigator->GetStatComponent(), Instigator->GetCombatComponent());

			if (USkeletalMeshComponent* MeshComp = Owner->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
				{
					// 다른 몽타쥬가 재생중이었다면, 추가처리하지않도록 한다.
					if (AnimInstance->IsAnyMontagePlaying())
					{
						EndAbilitySelf();
						return;
					}
				}
			}
		}
	}
	
	// 타겟 방향 회전
	FaceToAttacker(TriggerEventData->Instigator);

	// 메테리얼 Hit이펙트 적용
	if (bool HasHitReactMontage = 0 < MontagePaths.Num())
	{
		// 몽타쥬 재생
		UAnimMontage* MontageToPlay = GetMontageToPlay();
		if (nullptr == MontageToPlay)
		{
			EndAbilitySelf();
			return;
		}

		if (auto MontageTask =  PlayMontageWait(MontageToPlay))
		{
			MontageTask->ReadyForActivation();
		}
		
		SetHitFxSwitchParameter(1.0f);
		if(APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetOwningActorFromActorInfo()))
		{
			// TODO 여기 수정하자
			//Player->SetIsCanControl(false);
		}
	}
	else
	{
		SetHitFxSwitchParameter(1.0f);

		EndAbilitySelf();
	}

}

void UPGAbilityHitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Material Parameter 세팅
	SetHitFxSwitchParameter(0.0f);

	if(APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetOwningActorFromActorInfo()))
	{
		Player->SetIsCanControl(true);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UPGAbilityHitReact::GetMontageToPlay() const
{
	int32 Index = FMath::RandRange(0, MontagePaths.Num() - 1);
	if (MontagePaths.IsValidIndex(Index))
	{
		if (UObject* LoadedObject = MontagePaths[Index].TryLoad())
		{
			return Cast<UAnimMontage>(LoadedObject);
		}
	}
	return nullptr;
}

void UPGAbilityHitReact::FaceToAttacker(const AActor* Attacker)
{
	if (nullptr == Attacker || false == FaceToTarget)
	{
		return;
	}

	FVector AttackerLocation = Attacker->GetActorLocation();
	FVector OwnerLocation = GetOwningActorFromActorInfo()->GetActorLocation();
	AttackerLocation.Z = OwnerLocation.Z;
	
	FRotator NewRotator = UKismetMathLibrary::FindLookAtRotation(OwnerLocation, AttackerLocation);

	GetOwningActorFromActorInfo()->SetActorRotation(NewRotator);
}

void UPGAbilityHitReact::SetHitFxSwitchParameter(float Value)
{
	if (USkeletalMeshComponent* SkeletalMeshComp = GetOwningComponentFromActorInfo())
	{
		SkeletalMeshComp->SetScalarParameterValueOnMaterials(MaterialParameterName, Value);
	}
}

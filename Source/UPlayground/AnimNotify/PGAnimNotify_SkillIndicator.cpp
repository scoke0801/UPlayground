// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotify_SkillIndicator.h"

#include "AIController.h"
#include "PGDataTableManager.h"
#include "DataTable/Skill/PGSkillIndicatorDataRow.h"
#include "PGActor/Effects/Decal/PGSkillIndicator.h"
#include "Animation/AnimInstance.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "PGActor/Components/Combat/PGSkillMontageController.h"
#include "PGActor/Projectile/Pool/PGProjectileManager.h"

void UPGAnimNotify_SkillIndicator::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                          const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// 데이터테이블에서 ID값을 찾아서 생성
	UPGDataTableManager* DataTableManager = UPGDataTableManager::Get();
	if (nullptr == DataTableManager || nullptr == MeshComp)
	{
		return;
	}
	
	CachedOwner = MeshComp->GetOwner();
	if (nullptr == CachedOwner)
	{
		return;
	}

	if (FPGSkillIndicatorDataRow* Data = DataTableManager->GetRowData<FPGSkillIndicatorDataRow>(SkillIndicatorId))
	{
		// 클래스 로드
		if (UClass* LoadedClass = Data->IndicatorPath.TryLoadClass<AActor>())
		{
			UWorld* World = MeshComp->GetWorld();
			if (nullptr == World)
			{
				return;
			}
			// 스폰 위치 계산 (소유자 앞쪽)
			FVector OwnerLocation = CachedOwner->GetActorLocation();
			CachedDirection = CachedOwner->GetActorForwardVector();
			CachedLocation = OwnerLocation;
			
			FVector GroundLocation = GetGroundLocation(CachedOwner);
			FVector SpawnLocation = OwnerLocation + (CachedDirection * SpawnDistance) + SpawnOffset;
			
			SpawnLocation.Z = GroundLocation.Z;
			
			if (APGSkillIndicator* SpawnedActor = World->SpawnActor<APGSkillIndicator>(LoadedClass,
				SpawnLocation, CachedOwner->GetActorRightVector().Rotation()))
			{
				UPGSkillMontageController* MontageController = CachedOwner->FindComponentByClass<UPGSkillMontageController>();
				if (MontageController)
				{
					// 현재 재생 중인 몽타쥬 가져오기
					if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
					{
						UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
						if (CurrentMontage)
						{
							// 몽타쥬 컨트롤러에 인디케이터 등록
							MontageController->RegisterSkillIndicator(SpawnedActor, CurrentMontage);
							UE_LOG(LogTemp, Log, TEXT("스킬 인디케이터가 몽타쥬 컨트롤러에 등록됨"));
						}
					}
				}

				if (false == DecalSize.IsNearlyZero())
				{
					SpawnedActor->SetDecalSize(DecalSize);
				}
				SpawnedActor->SetAnimationDuration(IndicatorDuration);
				SpawnedActor->StartAnimation();

				// 인디케이터 완료 시 AOE 스폰 설정
				if (ProjectileId > 0)
				{
					SpawnedActor->OnAnimationCompleted.AddDynamic(this, &ThisClass::OnIndicatorCompleted);
				}
			}
		}
	}
}

AActor* UPGAnimNotify_SkillIndicator::GetTargetActor(AActor* OwnerActor) const
{
	if (nullptr == OwnerActor)
	{
		return nullptr;
	}

	// AI Controller에서 블랙보드 가져오기
	AAIController* AIController = Cast<AAIController>(OwnerActor->GetInstigatorController());
	if (nullptr == AIController)
	{
		return nullptr;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
	if (nullptr == Blackboard)
	{
		return nullptr;
	}

	// 블랙보드에서 타겟 액터 가져오기
	return Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKeyName));
}

FVector UPGAnimNotify_SkillIndicator::GetGroundLocation(AActor* TargetActor) const
{
	if (nullptr == TargetActor)
	{
		return FVector::ZeroVector;
	}

	FVector GroundLocation = TargetActor->GetActorLocation();

	// Character인 경우 CapsuleComponent의 반 높이를 고려
	if (ACharacter* Character = Cast<ACharacter>(TargetActor))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			GroundLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	return GroundLocation;
}

void UPGAnimNotify_SkillIndicator::OnIndicatorCompleted(APGSkillIndicator* Indicator)
{
	if (nullptr == CachedOwner || 0 == ProjectileId)
	{
		return;
	}
	
	if (UPGProjectileManager* Manager = PGProjectile())
	{
		Manager->FireProjectile(ProjectileId, CachedOwner, CachedLocation, CachedDirection);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAnimNotify_AOESkillIndicator.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "PGDataTableManager.h"
#include "DataTable/Skill/PGSkillIndicatorDataRow.h"
#include "PGActor/Effects/Decal/PGSkillIndicator.h"
#include "PGActor/AreaOfEffect/PGAreaOfEffectBase.h"
#include "Animation/AnimInstance.h"
#include "PGActor/Components/Combat/PGSkillMontageController.h"

void UPGAOESpawnHelper::OnIndicatorCompleted(APGSkillIndicator* Indicator)
{
	if (nullptr == Indicator || !OwnerActor.IsValid() || EffectId <= 0)
	{
		return;
	}

	// 인디케이터 위치에 AOE 스폰
	FVector AOELocation = Indicator->GetActorLocation();
	APGAreaOfEffectBase::Fire(OwnerActor.Get(), AOELocation, EffectId);
}

void UPGAnimNotify_AOESkillIndicator::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                          const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UPGDataTableManager* DataTableManager = UPGDataTableManager::Get();
	if (nullptr == DataTableManager || nullptr == MeshComp)
	{
		return;
	}
	
	AActor* OwnerActor = MeshComp->GetOwner();
	if (nullptr == OwnerActor)
	{
		return;
	}
	
	FPGSkillIndicatorDataRow* Data = DataTableManager->GetRowData<FPGSkillIndicatorDataRow>(SkillIndicatorId);
	if (nullptr == Data)
	{
		return;
	}

	UClass* LoadedClass = Data->IndicatorPath.TryLoadClass<AActor>();
	if (nullptr == LoadedClass)
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (nullptr == World)
	{
		return;
	}

	// 타겟 액터 가져오기
	AActor* TargetActor = GetTargetActor(OwnerActor);
	if (nullptr == TargetActor)
	{
		return;
	}

	// 타겟의 바닥 위치 계산
	FVector SpawnLocation = GetGroundLocation(TargetActor);

	// 인디케이터 스폰
	APGSkillIndicator* SpawnedIndicator = World->SpawnActor<APGSkillIndicator>(
		LoadedClass,
		SpawnLocation, 
		FRotator::ZeroRotator
	);

	if (nullptr == SpawnedIndicator)
	{
		return;
	}

	// 몽타쥬 컨트롤러에 인디케이터 등록
	if (UPGSkillMontageController* MontageController = OwnerActor->FindComponentByClass<UPGSkillMontageController>())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			if (UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage())
			{
				MontageController->RegisterSkillIndicator(SpawnedIndicator, CurrentMontage);
			}
		}
	}

	// 인디케이터 설정
	if (false == DecalSize.IsNearlyZero())
	{
		SpawnedIndicator->SetDecalSize(DecalSize);
	}
	SpawnedIndicator->SetAnimationDuration(IndicatorDuration);

	// 인디케이터 완료 시 AOE 스폰 설정
	if (AreaOfEffectId > 0)
	{
		UPGAOESpawnHelper* SpawnHelper = NewObject<UPGAOESpawnHelper>(SpawnedIndicator);
		SpawnHelper->OwnerActor = OwnerActor;
		SpawnHelper->EffectId = AreaOfEffectId;

		SpawnedIndicator->OnAnimationCompleted.AddDynamic(SpawnHelper, &UPGAOESpawnHelper::OnIndicatorCompleted);
	}

	// 애니메이션 시작
	SpawnedIndicator->StartAnimation();
}

AActor* UPGAnimNotify_AOESkillIndicator::GetTargetActor(AActor* OwnerActor) const
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

FVector UPGAnimNotify_AOESkillIndicator::GetGroundLocation(AActor* TargetActor) const
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

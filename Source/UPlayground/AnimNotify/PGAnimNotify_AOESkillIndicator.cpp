// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAnimNotify_AOESkillIndicator.h"

#include "PGDataTableManager.h"
#include "DataTable/Skill/PGSkillIndicatorDataRow.h"
#include "PGActor/Effects/Decal/PGSkillIndicator.h"
#include "Animation/AnimInstance.h"
#include "PGActor/Components/Combat/PGSkillMontageController.h"

void UPGAnimNotify_AOESkillIndicator::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                          const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// 데이터테이블에서 ID값을 찾아서 생성
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
			FVector OwnerLocation = OwnerActor->GetActorLocation();
			FVector OwnerForward = OwnerActor->GetActorForwardVector();
			
			FVector SpawnLocation = OwnerLocation;
   
			if (APGSkillIndicator* SpawnedActor = World->SpawnActor<APGSkillIndicator>(LoadedClass,
				SpawnLocation, OwnerActor->GetActorRightVector().Rotation()))
			{
				UPGSkillMontageController* MontageController = OwnerActor->FindComponentByClass<UPGSkillMontageController>();
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
						}
					}
				}

				if (false == DecalSize.IsNearlyZero())
				{
					SpawnedActor->SetDecalSize(DecalSize);
				}
				SpawnedActor->SetAnimationDuration(IndicatorDuration);
				SpawnedActor->StartAnimation();
			}
		}
	}
}


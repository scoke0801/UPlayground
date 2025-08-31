// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotify_SkillIndicator.h"

#include "PGDataTableManager.h"
#include "DataTable/Skill/PGSkillIndicatorDataRow.h"
#include "PGActor/Effects/Decal/PGSkillIndicator.h"

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
			
			FVector SpawnLocation = OwnerLocation + (OwnerForward * SpawnDistance) + SpawnOffset;
   
			if (APGSkillIndicator* SpawnedActor = World->SpawnActor<APGSkillIndicator>(LoadedClass,
				SpawnLocation, OwnerActor->GetActorRightVector().Rotation()))
			{
				SpawnedActor->StartAnimation();
			}
		}
	}
}

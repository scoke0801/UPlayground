// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTDecorator_IsPlayingMontage.h"

#include "AIController.h"

UPGBTDecorator_IsPlayingMontage::UPGBTDecorator_IsPlayingMontage()
{
	NodeName = "Check Montage Playing";
	bInvertCondition = true;
	
	// 조건이 변경될 때 노드를 다시 평가
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
}

bool UPGBTDecorator_IsPlayingMontage::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp,
                                                                 uint8* NodeMemory) const
{
	bool bIsMontageActive = false;
	
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		if (APawn* ControlledPawn = AIController->GetPawn())
		{
			// 메시 컴포넌트에서 애니메이션 인스턴스 얻기
			if (USkeletalMeshComponent* MeshComp = ControlledPawn->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
				{
					// 몽타쥬가 재생 중인지 확인
					bIsMontageActive = AnimInstance->IsAnyMontagePlaying();
				}
			}
		}
	}
	
	// 조건 반전 적용
	return bInvertCondition ? !bIsMontageActive : bIsMontageActive;
}

FString UPGBTDecorator_IsPlayingMontage::GetStaticDescription() const
{
	FString InvertText = bInvertCondition ? TEXT("NOT ") : TEXT("");
	return FString::Printf(TEXT("Is %s Montage Playing"), *InvertText);
}

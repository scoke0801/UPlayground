// Fill out your copyright notice in the Description page of Project Settings.

#include "PGSkillMontageController.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "PGActor/Effects/Decal/PGSkillIndicator.h"

UPGSkillMontageController::UPGSkillMontageController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGSkillMontageController::RegisterSkillIndicator(APGSkillIndicator* Indicator, UAnimMontage* Montage)
{
	if (!Indicator || !Montage)
		return;

	// 인디케이터를 대기 목록에 추가
	PendingIndicators.Add(MakeWeakObjectPtr(Indicator));
	CurrentMontage = MakeWeakObjectPtr(Montage);

	// 인디케이터 완료 이벤트 바인딩
	Indicator->OnAnimationCompleted.AddDynamic(this, &UPGSkillMontageController::OnSkillIndicatorCompleted);
}

void UPGSkillMontageController::OnSkillIndicatorCompleted(APGSkillIndicator* CompletedIndicator)
{
	if (!CompletedIndicator)
		return;

	// 완료된 인디케이터를 목록에서 제거
	PendingIndicators.RemoveAll([CompletedIndicator](const TWeakObjectPtr<APGSkillIndicator>& WeakPtr)
	{
		return !WeakPtr.IsValid() || WeakPtr.Get() == CompletedIndicator;
	});

	// 모든 인디케이터가 완료되었으면 몽타쥬 종료 섹션으로 이동
	if (AreAllIndicatorsCompleted())
	{
		FinishMontage();
	}
}

bool UPGSkillMontageController::AreAllIndicatorsCompleted() const
{
	// 유효한 인디케이터가 남아있지 않으면 모두 완료된 것으로 간주
	for (const TWeakObjectPtr<APGSkillIndicator>& WeakPtr : PendingIndicators)
	{
		if (WeakPtr.IsValid())
		{
			return false; // 아직 유효한 인디케이터가 남아있음
		}
	}
	return true;
}

int32 UPGSkillMontageController::GetPendingIndicatorCount() const
{
	int32 ValidCount = 0;
	for (const TWeakObjectPtr<APGSkillIndicator>& WeakPtr : PendingIndicators)
	{
		if (WeakPtr.IsValid())
		{
			ValidCount++;
		}
	}
	return ValidCount;
}

void UPGSkillMontageController::StartMontageLoop()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !CurrentMontage.IsValid())
		return;

	// 현재 몽타쥬가 재생 중인지 확인
	if (AnimInstance->Montage_IsPlaying(CurrentMontage.Get()))
	{
		// 루프 섹션으로 점프
		AnimInstance->Montage_JumpToSection(LoopSectionName, CurrentMontage.Get());
		
		// 루프 섹션이 무한 반복되도록 설정
		AnimInstance->Montage_SetNextSection(LoopSectionName, LoopSectionName, CurrentMontage.Get());
	}
}

void UPGSkillMontageController::FinishMontage()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !CurrentMontage.IsValid())
		return;

	if (AnimInstance->Montage_IsPlaying(CurrentMontage.Get()))
	{
		// 종료 섹션으로 점프
		AnimInstance->Montage_JumpToSection(EndSectionName, CurrentMontage.Get());
		
		// 루프 해제 (End 섹션은 자연스럽게 종료되도록)
		AnimInstance->Montage_SetNextSection(LoopSectionName, EndSectionName, CurrentMontage.Get());
	}

	// 인디케이터 목록 정리
	PendingIndicators.Empty();
	CurrentMontage.Reset();
}

UAnimInstance* UPGSkillMontageController::GetAnimInstance() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
		return nullptr;

	// 스켈레탈 메시 컴포넌트 찾기
	if (USkeletalMeshComponent* SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		return SkeletalMesh->GetAnimInstance();
	}

	return nullptr;
}

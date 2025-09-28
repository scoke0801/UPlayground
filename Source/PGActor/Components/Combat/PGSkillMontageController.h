// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimMontage.h"
#include "PGSkillMontageController.generated.h"

class APGSkillIndicator;

/**
 * 스킬 몽타쥬의 섹션 제어를 담당하는 컴포넌트
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PGACTOR_API UPGSkillMontageController : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGSkillMontageController();

protected:
	// 현재 대기 중인 스킬 인디케이터들
	UPROPERTY()
	TArray<TWeakObjectPtr<APGSkillIndicator>> PendingIndicators;

	// 현재 재생 중인 몽타쥬
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> CurrentMontage;

	// 몽타쥬 섹션 이름들
	UPROPERTY(EditDefaultsOnly, Category = "PG|Montage Sections")
	FName StartSectionName = TEXT("Start");

	UPROPERTY(EditDefaultsOnly, Category = "PG|Montage Sections")
	FName LoopSectionName = TEXT("Loop");

	UPROPERTY(EditDefaultsOnly, Category = "PG|Montage Sections")
	FName EndSectionName = TEXT("End");

public:
	// 스킬 인디케이터 등록
	UFUNCTION(BlueprintCallable, Category = "PG|Skill")
	void RegisterSkillIndicator(APGSkillIndicator* Indicator, UAnimMontage* Montage);

	// 스킬 인디케이터 완료 처리
	UFUNCTION(BlueprintCallable, Category = "PG|Skill")
	void OnSkillIndicatorCompleted(APGSkillIndicator* CompletedIndicator);

	// 모든 인디케이터가 완료되었는지 확인
	UFUNCTION(BlueprintCallable, Category = "PG|Skill")
	bool AreAllIndicatorsCompleted() const;

	// 현재 대기 중인 인디케이터 수
	UFUNCTION(BlueprintCallable, Category = "PG|Skill")
	int32 GetPendingIndicatorCount() const;

private:
	// 몽타쥬를 루프 섹션으로 이동
	void StartMontageLoop();

	// 몽타쥬를 종료 섹션으로 이동
	void FinishMontage();

	// 애니메이션 인스턴스 가져오기
	class UAnimInstance* GetAnimInstance() const;
};

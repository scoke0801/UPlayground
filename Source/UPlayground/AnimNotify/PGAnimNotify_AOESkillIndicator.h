// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PGAnimNotify_AOESkillIndicator.generated.h"

class UPGSkillMontageController;
class APGSkillIndicator;

/**
 * AOE 스폰 헬퍼 - 인디케이터 완료 시 AOE를 스폰하는 콜백 핸들러
 */
UCLASS()
class UPLAYGROUND_API UPGAOESpawnHelper : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TWeakObjectPtr<AActor> OwnerActor;
	
	int32 EffectId = 0;

	UFUNCTION()
	void OnIndicatorCompleted(APGSkillIndicator* Indicator);
};

/**
 * AOE 스킬 인디케이터 애님 노티파이
 * 스킬 타겟의 바닥 위치에 인디케이터를 생성하고, 애니메이션 완료 시 AOE 이펙트를 스폰합니다.
 */
UCLASS()
class UPLAYGROUND_API UPGAnimNotify_AOESkillIndicator : public UAnimNotify
{
	GENERATED_BODY()

protected:
	// 스킬 인디케이터 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	int32 SkillIndicatorId;

	// 인디케이터 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	float IndicatorDuration = 1.0f;
	
	// 데칼 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	FVector DecalSize = FVector::ZeroVector;

	// 인디케이터 완료 후 스폰할 AOE 이펙트 ID( 사용 안하고 직접 AnimNotify_AOE도 가능 )
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AOE Settings")
	int32 AreaOfEffectId = 0;

	// 타겟을 저장한 블랙보드 키 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Target Settings")
	FName TargetActorKeyName = FName("TargetActor");

private:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// 타겟 액터 위치 가져오기
	AActor* GetTargetActor(AActor* OwnerActor) const;
	
	// 타겟의 바닥 위치 계산
	FVector GetGroundLocation(AActor* TargetActor) const;
};

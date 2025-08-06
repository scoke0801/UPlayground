// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISenseConfig_Sight.h"
#include "PGAIController.generated.h"

/**
 * 
 */
UCLASS()
class PGACTOR_API APGAIController : public AAIController
{
	GENERATED_BODY()
	
protected:
	/** AI의 인식 시스템을 담당하는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UAIPerceptionComponent> EnemyPerceptionComponent;

	/** 시야 기반 감지 설정을 담당하는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UAISenseConfig_Sight> AISenseConfig_Sight;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config")
	bool bEnableDetourCrowdAvoidance = true;

	//bEnableDetourCrowdAvoidance가 true인 경우에만 편집 가능하도록 meta 데이터 설정
	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config", meta = (EditCondition = "bEnableDetourCrowdAvoidance", UIMin = "1", UIMax = "4"))
	int32 DetourCrowdAvoidanceQuality = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config", meta = (EditCondition = "bEnableDetourCrowdAvoidance"))
	float CollisionQueryRange = 600.f;

	
public:
	/** AI 컨트롤러 생성자 - AI 인식 시스템과 경로 찾기 컴포넌트를 초기화합니다 */
	APGAIController(const FObjectInitializer& ObjectInitializer);

protected:
	// AActor
	virtual void BeginPlay() override;
	
public:
	// IGenericTeamAgentInterface
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	
protected:
	/** 적 인식 정보가 업데이트될 때 호출되는 콜백 함수 */
	UFUNCTION()
	virtual void OnEnemyPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};

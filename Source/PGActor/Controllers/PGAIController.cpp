// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

APGAIController::APGAIController(const FObjectInitializer& ObjectInitializer)
{
	// AI 시야 감지 설정 컴포넌트 생성
	AISenseConfig_Sight = CreateDefaultSubobject<UAISenseConfig_Sight>("EnemySenseConfigSight");
	if (nullptr != AISenseConfig_Sight)
	{
		// 감지 대상 설정: 적만 감지하고 아군과 중립은 무시
		AISenseConfig_Sight->DetectionByAffiliation.bDetectEnemies = true;   // 적 감지 활성화
		AISenseConfig_Sight->DetectionByAffiliation.bDetectFriendlies = false; // 아군 감지 비활성화
		AISenseConfig_Sight->DetectionByAffiliation.bDetectNeutrals = false;   // 중립 감지 비활성화
		
		// 시야 범위 설정
		AISenseConfig_Sight->SightRadius = 5000.f;                    // 감지 가능한 최대 거리 (5000 유닛)
		AISenseConfig_Sight->LoseSightRadius = 0.f;                   // 시야를 잃는 거리 (0 = SightRadius와 동일)
		AISenseConfig_Sight->PeripheralVisionAngleDegrees = 360.f;    // 시야각 (360도 = 전방향 감지)
	}

	// AI 인식 컴포넌트 생성 및 설정
	EnemyPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>("EnemyPerceptionComponent");
	if (nullptr != EnemyPerceptionComponent)
	{
		// 위에서 설정한 시야 센스를 인식 컴포넌트에 적용
		EnemyPerceptionComponent->ConfigureSense(*AISenseConfig_Sight);
		// 시야 센스를 주요 감지 방식으로 설정
		EnemyPerceptionComponent->SetDominantSense(UAISenseConfig_Sight::StaticClass());
		// 대상 인식 업데이트 시 호출될 콜백 함수 등록
		EnemyPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this,
			&ThisClass::OnEnemyPerceptionUpdated);
	}

	SetGenericTeamId(FGenericTeamId(1));
}

void APGAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		ECrowdSimulationState CrowdSimulationState = bEnableDetourCrowdAvoidance
			? ECrowdSimulationState::Enabled
			: ECrowdSimulationState::Disabled;
		CrowdComp->SetCrowdSimulationState(CrowdSimulationState);

		switch (DetourCrowdAvoidanceQuality)
		{
		case 1: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);break;
		case 2: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);break;
		case 3: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);break;
		case 4: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);break;
		default: break;
		}

		CrowdComp->SetAvoidanceGroup(1);
		CrowdComp->SetGroupsToAvoid(1);
		CrowdComp->SetCrowdCollisionQueryRange(CollisionQueryRange);
	}
}

ETeamAttitude::Type APGAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const APawn* PawnToCheck = Cast<const APawn>(&Other);

	if (PawnToCheck)
	{
		const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(PawnToCheck->GetController());
		if (OtherTeamAgent && OtherTeamAgent->GetGenericTeamId() < GetGenericTeamId())
		{
			// ID가 다른 경우 적으로 판단
			return ETeamAttitude::Hostile;
		}
	}

	return ETeamAttitude::Friendly;
}

void APGAIController::OnEnemyPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
	{
		if (!BlackboardComponent->GetValueAsObject((FName("TargetActor"))))
		{
			if (Stimulus.WasSuccessfullySensed() && Actor)
			{
				BlackboardComponent->SetValueAsObject(FName("TargetActor"), Actor);
			}
		}
	}
}

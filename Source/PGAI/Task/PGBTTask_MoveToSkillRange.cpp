// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTTask_MoveToSkillRange.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

UPGBTTask_MoveToSkillRange::UPGBTTask_MoveToSkillRange()
{
	NodeName = "Move To Skill Range";
	bNotifyTick = false;  // Tick 사용 안함
	bNotifyTaskFinished = true;
	
	TargetActorKey.SelectedKeyName = "TargetActor";
	SelectedSkillIDKey.SelectedKeyName = "SelectedSkillId";
}

EBTNodeResult::Type UPGBTTask_MoveToSkillRange::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	// 타겟 액터 가져오기
	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	// 선택된 스킬 ID 가져오기
	const int32 SkillID = Blackboard->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName);
	if (SkillID <= 0)
	{
		return EBTNodeResult::Failed;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		return EBTNodeResult::Failed;
	}

	// 스킬 데이터에서 사용 범위 가져오기
	CachedSkillRange = GetSkillRangeFromData(SkillID);
	if (CachedSkillRange <= 0.0f)
	{
		// 범위 제한이 없는 스킬이면 즉시 성공
		return EBTNodeResult::Succeeded;
	}

	// 현재 거리 계산
	const float CurrentDistance = FVector::Dist(
		ControlledPawn->GetActorLocation(), 
		TargetActor->GetActorLocation()
	);
	
	// 목표 거리 (스킬 범위에서 버퍼를 뺀 값)
	const float TargetDistance = FMath::Max(CachedSkillRange - RangeBuffer, 0.0f);

	// 이미 범위 내부라면 즉시 성공
	if (CurrentDistance <= (CachedSkillRange + AcceptableRadiusMargin))
	{
		return EBTNodeResult::Succeeded;
	}

	// BehaviorTreeComponent 캐싱
	CachedOwnerComp = &OwnerComp;

	// 범위 밖이면 이동 시작
	FAIMoveRequest MoveRequest(TargetActor);
	MoveRequest.SetAcceptanceRadius(TargetDistance);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);

	FPathFollowingRequestResult Result = AIController->MoveTo(MoveRequest);
	
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		MoveRequestID = Result.MoveId;
		
		// 이동 완료 콜백 등록
		if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
		{
			PathFollowingComp->OnRequestFinished.AddUObject(
				this, 
				&UPGBTTask_MoveToSkillRange::OnMoveCompleted
			);
		}

		// 타이머 기반 거리 체크 시작
		if (UWorld* World = OwnerComp.GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DistanceCheckTimerHandle,
				this,
				&UPGBTTask_MoveToSkillRange::CheckDistanceToTarget,
				DistanceCheckInterval,
				true  // 반복
			);
		}
		
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

EBTNodeResult::Type UPGBTTask_MoveToSkillRange::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		// 진행 중인 이동 중단
		AIController->StopMovement();
		
		if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
		{
			PathFollowingComp->OnRequestFinished.RemoveAll(this);
		}
	}

	// 타이머 정리
	ClearDistanceCheckTimer();

	return EBTNodeResult::Aborted;
}

void UPGBTTask_MoveToSkillRange::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	// 콜백 정리
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
		{
			PathFollowingComp->OnRequestFinished.RemoveAll(this);
		}
	}

	// 타이머 정리
	ClearDistanceCheckTimer();

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UPGBTTask_MoveToSkillRange::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	// 이 요청이 우리가 생성한 것인지 확인
	if (RequestID != MoveRequestID)
	{
		return;
	}

	// BehaviorTreeComponent 가져오기
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	if (!OwnerComp)
	{
		return;
	}

	// 타이머 정리
	ClearDistanceCheckTimer();

	// 이동 결과에 따라 태스크 완료 처리
	if (Result.IsSuccess())
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
	else if (Result.IsInterrupted())
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Aborted);
	}
	else
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
	}
}

void UPGBTTask_MoveToSkillRange::CheckDistanceToTarget()
{
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	if (!OwnerComp)
	{
		ClearDistanceCheckTimer();
		return;
	}

	AAIController* AIController = OwnerComp->GetAIOwner();
	if (!AIController)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp->GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	// 타겟 액터 가져오기
	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// 현재 거리 계산
	const float CurrentDistance = FVector::Dist(
		ControlledPawn->GetActorLocation(), 
		TargetActor->GetActorLocation()
	);

	// 스킬 범위 내로 들어왔으면 이동 중단하고 성공 처리
	if (CurrentDistance <= (CachedSkillRange + AcceptableRadiusMargin))
	{
		// 타이머 정리
		ClearDistanceCheckTimer();

		// 이동 중단
		AIController->StopMovement();

		// PathFollowing 콜백 제거
		if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
		{
			PathFollowingComp->OnRequestFinished.RemoveAll(this);
		}
		
		// 성공 처리
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
}

void UPGBTTask_MoveToSkillRange::ClearDistanceCheckTimer()
{
	if (UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get())
	{
		if (UWorld* World = OwnerComp->GetWorld())
		{
			if (DistanceCheckTimerHandle.IsValid())
			{
				World->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
				DistanceCheckTimerHandle.Invalidate();
			}
		}
	}
}

float UPGBTTask_MoveToSkillRange::GetSkillRangeFromData(int32 SkillID) const
{
	// 데이터 테이블에서 스킬 정보 가져오기
	if (UPGDataTableManager* DataTableManager = UPGDataTableManager::Get())
	{
		if (const FPGSkillDataRow* SkillData = DataTableManager->GetRowData<FPGSkillDataRow>(SkillID))
		{
			// SkillRange가 0이면 범위 제한 없음
			if (SkillData->SkillRange > 0.0f)
			{
				return SkillData->SkillRange;
			}
		}
	}

	// 데이터를 찾지 못했거나 범위가 0이면 기본값 반환
	return 0.0f;
}

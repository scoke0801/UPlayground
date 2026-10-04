// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTTask_FindSkillUseLocation.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

UPGBTTask_FindSkillUseLocation::UPGBTTask_FindSkillUseLocation()
{
	NodeName = "Find Skill Use Location (EQS)";
	bNotifyTick = false;
	bNotifyTaskFinished = true;
	bCreateNodeInstance = true;
	
	TargetActorKey.SelectedKeyName = "TargetActor";
	SelectedSkillIDKey.SelectedKeyName = "SelectedSkillId";
	SkillLocationKey.SelectedKeyName = "SkillUseLocation";
}

EBTNodeResult::Type UPGBTTask_FindSkillUseLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CancelQuery();
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
	Blackboard->ClearValue(SkillLocationKey.SelectedKeyName);

	// EQS 쿼리가 설정되지 않았으면 실패
	if (!FindSkillLocationQuery)
	{
		UE_LOG(LogTemp, Error, TEXT("FindSkillLocationQuery is not set!"));
		return EBTNodeResult::Failed;
	}

	// 타겟 액터 확인
	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("TargetActor is null!"));
		return EBTNodeResult::Failed;
	}

	// 선택된 스킬 ID 가져오기
	const int32 SkillID = Blackboard->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName);
	if (SkillID <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid SkillID: %d"), SkillID);
		return EBTNodeResult::Failed;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		return EBTNodeResult::Failed;
	}

	// 스킬 데이터에서 최대 거리 가져오기
	auto* Tables = UPGDataTableManager::Get(ControlledPawn);
	const auto* Skill = Tables ? Tables->GetSkillDataRowByKey(SkillID) : nullptr;
	if (!Skill || (Skill->TelegraphDuration > 0.f && !Skill->IsPatternValid())) return EBTNodeResult::Failed;
	const float MaxDistance = GetMaxDistanceFromSkillData(SkillID);
	if (FMath::IsNearlyZero(MaxDistance))
	{
		// 스킬 사거리가 지정되지 않았다면 별도로 이동할 필요는 없음
		Blackboard->SetValueAsVector(SkillLocationKey.SelectedKeyName,
			ControlledPawn->GetActorLocation());
		return EBTNodeResult::Succeeded;
	}
	
	// BehaviorTreeComponent 캐싱
	CachedOwnerComp = &OwnerComp;
	QueryTarget = TargetActor;
	QuerySkillID = SkillID;

	// EQS 요청 생성
	FEnvQueryRequest QueryRequest(FindSkillLocationQuery, ControlledPawn);

	// Named Parameter 설정
	QueryRequest.SetFloatParam(FName("MaxDistance"), MaxDistance);
	QueryRequest.SetFloatParam(FName("MinDistance"), Skill->MinimumActivationRange);
	
	// EQS 비동기 실행
	QueryID = QueryRequest.Execute(
		EEnvQueryRunMode::SingleResult,
		this,
		&UPGBTTask_FindSkillUseLocation::OnEQSQueryFinished
	);

	if (QueryID == INDEX_NONE) { CancelQuery(); return EBTNodeResult::Failed; }
	return EBTNodeResult::InProgress;
}

void UPGBTTask_FindSkillUseLocation::CancelQuery()
{
	const int32 RequestID = QueryID;
	UWorld* World = CachedOwnerComp.IsValid() ? CachedOwnerComp->GetWorld() : nullptr;
	// Invalidate first: AbortQuery may synchronously dispatch its completion delegate.
	QueryID = INDEX_NONE;
	QuerySkillID = 0;
	QueryTarget.Reset();
	CachedOwnerComp.Reset();
	if (RequestID != INDEX_NONE && World)
		if (auto* Manager = UEnvQueryManager::GetCurrent(World)) Manager->AbortQuery(RequestID);
}

EBTNodeResult::Type UPGBTTask_FindSkillUseLocation::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CancelQuery();
	return EBTNodeResult::Aborted;
}

void UPGBTTask_FindSkillUseLocation::OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp)
{
	CancelQuery();
	Super::OnInstanceDestroyed(OwnerComp);
}

void UPGBTTask_FindSkillUseLocation::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	CancelQuery();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UPGBTTask_FindSkillUseLocation::OnEQSQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	if (!OwnerComp || QueryID == INDEX_NONE || !Result.IsValid() || Result->QueryID != QueryID)
	{
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp->GetBlackboardComponent();
	const bool bCurrentContext = QueryTarget.IsValid() && Blackboard &&
		Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName) == QueryTarget.Get() &&
		Blackboard->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName) == QuerySkillID;
	QueryID = INDEX_NONE;
	CachedOwnerComp.Reset();
	QueryTarget.Reset();
	QuerySkillID = 0;
	if (!bCurrentContext || !Result->IsSuccessful() || Result->Items.IsEmpty())
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 결과 위치 가져오기
	FVector SkillUseLocation = Result->GetItemAsLocation(0);
	
	// Blackboard에 결과 위치 저장
	if (Blackboard)
	{
		Blackboard->SetValueAsVector(SkillLocationKey.SelectedKeyName, SkillUseLocation);
		
		// 성공
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
	else
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
	}
}

float UPGBTTask_FindSkillUseLocation::GetMaxDistanceFromSkillData(int32 SkillID) const
{
	if (UPGDataTableManager* DataTableManager = UPGDataTableManager::Get())
	{
		if (const FPGSkillDataRow* SkillData = DataTableManager->GetRowData<FPGSkillDataRow>(SkillID))
		{
			const float Range = SkillData->GetPatternActivationRange();
			if (Range < TNumericLimits<float>::Max())
			{
				return Range;
			}
		}
	}

	return 0.0f;
}

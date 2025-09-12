// Fill out your copyright notice in the Description page of Project Settings.


#include "PGBTTask_RotateToFaceTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Kismet/KismetMathLibrary.h"

bool FRotateToFaceTargetTaskMemory::IsValid() const
{
	return OwningPawn.IsValid() && TargetActor.IsValid();
}

void FRotateToFaceTargetTaskMemory::Reset()
{
	OwningPawn.Reset();
	TargetActor.Reset();
}

UPGBTTask_RotateToFaceTarget::UPGBTTask_RotateToFaceTarget()
{
	// Behavior Tree 에디터에서 표시될 노드 이름 설정
	NodeName = TEXT("Native Rotate To Face Target Actor");
	// 회전 완료로 간주할 각도 오차의 기본값 (10도)
	AnglePrecision = 10.f;
	// 회전 보간 속도의 기본값 (5.0)
	RotationInterpSpeed = 5.f;

	// 매 틱마다 TickTask 함수 호출을 받도록 설정
	bNotifyTick = true;
	// 태스크 완료 시 OnTaskFinished 함수 호출을 받도록 설정
	bNotifyTaskFinished = true;
	// 노드 인스턴스를 별도로 생성하지 않음 (메모리 효율성)
	bCreateNodeInstance = false;

	// 태스크 노드의 알림 플래그들을 초기화하는 매크로
	INIT_TASK_NODE_NOTIFY_FLAGS();

	// 블랙보드 키 선택기에 AActor 클래스 타입 필터를 추가하여 액터만 선택 가능하도록 제한
	InTargetToFaceKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(ThisClass, InTargetToFaceKey), AActor::StaticClass());
}

void UPGBTTask_RotateToFaceTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		InTargetToFaceKey.ResolveSelectedKey((*BBAsset));
	}
}

uint16 UPGBTTask_RotateToFaceTarget::GetInstanceMemorySize() const
{
	return sizeof(FRotateToFaceTargetTaskMemory);
}

FString UPGBTTask_RotateToFaceTarget::GetStaticDescription() const
{
	const FString KeyDescString = InTargetToFaceKey.SelectedKeyName.ToString();

	return FString::Printf(TEXT("Smoothly to face %s key until the angle precision %s is reached"),
		*KeyDescString, *FString::SanitizeFloat(AnglePrecision));
}

/**
 * 태스크 실행 시작 시 호출되는 함수
 * 블랙보드에서 타겟 액터를 가져오고, 메모리에 필요한 정보를 저장한 후 초기 상태를 판단
 * @param OwnerComp 이 태스크를 실행하는 Behavior Tree 컴포넌트
 * @param NodeMemory 이 태스크 인스턴스의 메모리 포인터
 * @return 태스크 실행 결과 (성공, 실패, 진행중)
 */
EBTNodeResult::Type UPGBTTask_RotateToFaceTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 블랙보드에서 타겟 액터 오브젝트를 가져옴
	UObject* ActorObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(InTargetToFaceKey.SelectedKeyName);

	// 가져온 오브젝트를 AActor 타입으로 캐스팅
	AActor* TargetActor = Cast<AActor>(ActorObject);

	// AI 컨트롤러가 조종하는 폰을 가져옴
	APawn* OwningPawn = OwnerComp.GetAIOwner()->GetPawn();

	// 노드 메모리를 태스크 메모리 구조체로 캐스팅
	FRotateToFaceTargetTaskMemory* Memory = CastInstanceNodeMemory<FRotateToFaceTargetTaskMemory>(NodeMemory);

	// 메모리에 폰과 타겟 액터 저장
	Memory->OwningPawn = OwningPawn;
	Memory->TargetActor = TargetActor;

	// 폰과 타겟 액터가 모두 유효한지 확인
	if (false == Memory->IsValid())
	{
		return EBTNodeResult::Failed;
	}
	
	// 이미 목표 각도에 도달했는지 확인
	if (HasReachedAnglePrecision(OwningPawn, TargetActor))
	{
		Memory->Reset();
		return EBTNodeResult::Succeeded;
	}

	// 회전이 필요하므로 진행중 상태 반환
	return EBTNodeResult::InProgress;
}

/**
 * 매 프레임 호출되어 실제 회전 동작을 수행하는 함수
 * 목표 각도에 도달했는지 확인하고, 도달하지 않았다면 타겟 방향으로 점진적으로 회전
 * @param OwnerComp 이 태스크를 실행하는 Behavior Tree 컴포넌트
 * @param NodeMemory 이 태스크 인스턴스의 메모리 포인터
 * @param DeltaSeconds 이전 프레임과의 시간 차이
 */
void UPGBTTask_RotateToFaceTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	// 노드 메모리를 태스크 메모리 구조체로 캐스팅
	FRotateToFaceTargetTaskMemory* Memory = CastInstanceNodeMemory<FRotateToFaceTargetTaskMemory>(NodeMemory);

	// 메모리에 저장된 오브젝트들이 여전히 유효한지 확인
	if (false == Memory->IsValid())
	{
		// 유효하지 않으면 태스크 실패로 종료
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);	
	}
	
	// 목표 각도 정밀도에 도달했는지 확인
	if (HasReachedAnglePrecision(Memory->OwningPawn.Get(), Memory->TargetActor.Get()))
	{
		// 목표에 도달했으면 메모리 정리하고 성공으로 태스크 종료
		Memory->Reset();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
	else
	{
		// 높이 차이를 무시하기 위해 Z값을 같게 맞춤
		FVector OwnerLocation = Memory->OwningPawn->GetActorLocation();
		FVector TargetLocation = Memory->TargetActor->GetActorLocation();
		TargetLocation.Z = OwnerLocation.Z; // 타겟의 높이를 폰의 높이와 같게 설정

		// 폰 위치에서 타겟을 바라보는 회전값 계산 (수평면에서만)
		const FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(OwnerLocation, TargetLocation);

		// 현재 회전에서 목표 회전으로 부드럽게 보간
		const FRotator TargetRot = FMath::RInterpTo(Memory->OwningPawn->GetActorRotation(),
			LookAtRot, DeltaSeconds, RotationInterpSpeed);

		// 계산된 회전값을 폰에 적용
		Memory->OwningPawn->SetActorRotation(TargetRot);
	}
}

/**
 * 목표 각도 정밀도에 도달했는지 확인하는 함수
 * 폰의 전방 벡터와 타겟 방향 벡터 사이의 각도를 계산하여 설정된 정밀도 범위 내인지 판단
 * @param QueryPawn 회전을 확인할 폰
 * @param TargetActor 바라봐야 할 타겟 액터
 * @return 설정된 각도 정밀도 범위 내에 있으면 true
 */
bool UPGBTTask_RotateToFaceTarget::HasReachedAnglePrecision(APawn* QueryPawn, AActor* TargetActor)
{
	// 폰의 전방 방향 벡터를 가져옴
	FVector OwnerForward = QueryPawn->GetActorForwardVector();
	OwnerForward.Z = 0.0f;
	OwnerForward.Normalize();

	// 폰에서 타겟으로의 방향 벡터 계산 (높이 차이 무시)
	FVector OwnerToTarget = TargetActor->GetActorLocation() - QueryPawn->GetActorLocation();
	OwnerToTarget.Z = 0.0f;
	const FVector OwnerToTargetNormalized = OwnerToTarget.GetSafeNormal();

	// 두 벡터의 내적을 계산 (코사인 값)
	const float DotResult = FVector::DotProduct(OwnerForward, OwnerToTargetNormalized);

	// 내적 결과를 이용해 두 벡터 사이의 각도를 계산 (도 단위)
	const float AngleDiff = UKismetMathLibrary::DegAcos(DotResult);

	// 계산된 각도 차이가 설정된 각도 정밀도 이하인지 확인
	return AngleDiff <= AnglePrecision;
}

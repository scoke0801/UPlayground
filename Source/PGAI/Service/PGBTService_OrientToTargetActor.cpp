// Fill out your copyright notice in the Description page of Project Settings.


#include "PGBTService_OrientToTargetActor.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Kismet/KismetMathLibrary.h"

UPGBTService_OrientToTargetActor::UPGBTService_OrientToTargetActor()
{
	NodeName = TEXT("Native Orient Rotation To Target Actor");

	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	RotationInterpSpeed = 5.f;
	Interval = 0.f;

	RandomDeviation = 0.f;

	InTargetActorKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(ThisClass, InTargetActorKey),
		AActor::StaticClass());
}

void UPGBTService_OrientToTargetActor::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		InTargetActorKey.ResolveSelectedKey(*BBAsset);
	}
}

FString UPGBTService_OrientToTargetActor::GetStaticDescription() const
{
	const FString KeyDescString = InTargetActorKey.SelectedKeyName.ToString();

	return FString::Printf(TEXT("Orient rotation to %s Key %s"), *KeyDescString, *GetStaticServiceDescription());
}

void UPGBTService_OrientToTargetActor::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UObject* ActorObject =OwnerComp.GetBlackboardComponent()->GetValueAsObject(InTargetActorKey.SelectedKeyName);

	AActor* TargetActor = Cast<AActor>(ActorObject);

	APawn* OwningPawn = OwnerComp.GetAIOwner()->GetPawn();
	if (OwningPawn && TargetActor)
	{
		const FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			OwningPawn->GetActorLocation(), TargetActor->GetActorLocation());

		const FRotator TargetRot = FMath::RInterpTo(OwningPawn->GetActorRotation(),
			LookAtRot, DeltaSeconds, RotationInterpSpeed);

		OwningPawn->SetActorRotation(LookAtRot);
	}
}

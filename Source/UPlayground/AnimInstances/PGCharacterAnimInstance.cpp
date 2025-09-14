


#include "PGCharacterAnimInstance.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"

void UPGCharacterAnimInstance::NativeInitializeAnimation()
{
	// Initialize variables
	Acceleration = FVector::ZeroVector;
	bHasAcceleration = false;
	Velocity = FVector::ZeroVector;
	bHasVelocity = false;
	DisplacementSinceLastUpdate = 0.0;
	CurrentWorldLocation = FVector::ZeroVector;
	DisplacementSpeed = 0.0f;
	PoseWrappingEnum = EPGLocomotionDirection::Forward;
}

void UPGCharacterAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();

	OwningCharacter = Cast<APGCharacterBase>(TryGetPawnOwner());
	
	if (OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
	}
}

void UPGCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	if (nullptr == OwningCharacter || nullptr == OwningMovementComponent)
	{
		return;
	}
	UpdateHasAcceleration();
	UpdateHasVelocity();
	UpdateDisplacementSpeed(DeltaSeconds);
	UpdateLocomotionDirection();

	UpdateIsFalling();
	UpdateIsJumping();
}

void UPGCharacterAnimInstance::UpdateHasAcceleration()
{
	FVector CurrentAcceleration = OwningMovementComponent->GetCurrentAcceleration();

	// Z축 제거 (1,1,0 벡터와 곱하기)
	FVector MaskVector(1.0f, 1.0f, 0.0f);
	Acceleration = CurrentAcceleration;

	// 가속도가 거의 0인지 확인하고 반전
	bool bIsNearlyZero = UKismetMathLibrary::Vector_IsNearlyZero(Acceleration, 0.0001f);
	bHasAcceleration = !bIsNearlyZero;
}

void UPGCharacterAnimInstance::UpdateHasVelocity()
{
	FVector CurrentVelocity = OwningCharacter->GetVelocity();
    
	// Z축 제거 (1,1,0 벡터와 곱하기)
	FVector MaskVector(1.0f, 1.0f, 0.0f);
	Velocity = CurrentVelocity * MaskVector;
    
	// 속도가 거의 0인지 확인하고 반전
	bool bIsNearlyZero = UKismetMathLibrary::Vector_IsNearlyZero(Velocity, 0.0001f);
	bHasVelocity = !bIsNearlyZero;
}

void UPGCharacterAnimInstance::UpdateLocomotionDirection()
{
}

void UPGCharacterAnimInstance::UpdateDisplacementSpeed(float DeltaSeconds)
{
	// 위치 기반 속도 계산 (필요한 경우)
	const FVector NewWorldLocation = OwningCharacter->GetActorLocation();
	DisplacementSinceLastUpdate = FVector::Dist2D(CurrentWorldLocation, NewWorldLocation);
	DisplacementSpeed = DisplacementSinceLastUpdate / DeltaSeconds;
    
	// 다음 프레임을 위해 현재 위치 저장
	CurrentWorldLocation = NewWorldLocation;
}

void UPGCharacterAnimInstance::UpdateIsFalling()
{
	bool PrevState = bIsOnFalling;
	bIsOnFalling = OwningMovementComponent->IsFalling();
	if (bIsOnFalling && PrevState != bIsOnFalling)
	{
		// 떨어진다면, 점프상태로 변경
		if (APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(OwningCharacter))
		{
			// 너무 낮으면 안뜀
			if (MinJumpHeight <= GetDistanceToGround())
			{
				Player->SetIsJump(true);
			}
		}
	}
}

void UPGCharacterAnimInstance::UpdateIsJumping()
{
	if (APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(OwningCharacter))
	{
		bIsJumping = Player->GetIsJumping();
	}
}

float UPGCharacterAnimInstance::GetDistanceToGround()
{
	if (!OwningCharacter)
	{
		return 0.0f;
	}

	// 캐릭터의 발 위치에서 시작 (Capsule 하단)
	FVector StartLocation = OwningCharacter->GetActorLocation();
	if (UCapsuleComponent* Capsule = OwningCharacter->GetCapsuleComponent())
	{
		float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		StartLocation.Z -= CapsuleHalfHeight; // 캡슐 하단으로 조정
	}
    
	// 아래쪽으로 LineTrace
	FVector EndLocation = StartLocation - FVector(0, 0, GroundCheckDistance);
    
	// Collision 설정
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwningCharacter);
	QueryParams.bTraceComplex = false;
    
	// LineTrace 실행
	FHitResult HitResult;
	bool bHit = OwningCharacter->GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECC_WorldStatic, // 지면만 체크 (WorldStatic)
		QueryParams
	);

// #if WITH_EDITOR
// 	FVector MinHeightPoint = StartLocation - FVector(0, 0, MinJumpHeight);
// 	DrawDebugSphere(
// 		OwningCharacter->GetWorld(),
// 		MinHeightPoint,
// 		20.0f,
// 		12,
// 		FColor::Yellow,
// 		false,
// 		0.1f
// 	);
// #endif
	
	if (bHit)
	{
		// 지면까지의 거리 반환
		float Distance = FVector::Dist(StartLocation, HitResult.Location);
		return Distance;
	}
    
	// 지면을 찾지 못한 경우 최대 거리 반환
	return GroundCheckDistance;
}


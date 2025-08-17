


#include "PGCharacterAnimInstance.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
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
}

void UPGCharacterAnimInstance::SetIsFalling(bool InIsFalling)
{
	bIsJumping = InIsFalling;
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
	if (APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(OwningCharacter))
	{
		bIsJumping = Player->GetIsJumping();
	}

	bool CurrentIsFalling =OwningMovementComponent->IsFalling();
	if (true == bIsOnFalling && false == CurrentIsFalling)
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			GetOwningActor(),
			PGGamePlayTags::Player_Event_JumpLanded,
			FGameplayEventData()
		);
		bIsJumping = false;
	}
	bIsOnFalling = OwningMovementComponent->IsFalling();
}

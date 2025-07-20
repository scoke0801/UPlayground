


#include "PGCharacterAnimInstance.h"
#include "AnimInstances/PGCharacterAnimInstance.h"

#include "KismetAnimationLibrary.h"
#include "Characters/LocalPlayer/PGLocalPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UPGCharacterAnimInstance::NativeInitializeAnimation()
{
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
	UpdateLocomotionAngle();
	UpdateLocomotionDirection();
}

void UPGCharacterAnimInstance::UpdateHasVelocity()
{
	FVector2D Velocity2D = FVector2D(OwningCharacter->GetVelocity());
	CurrentVelocity = Velocity2D.Size();

	bHasVelocity = !Velocity2D.IsNearlyZero();
}

void UPGCharacterAnimInstance::UpdateHasAcceleration()
{
	bHasAcceleration =
		(false == FVector2D(OwningMovementComponent->GetCurrentAcceleration().Size2D()).IsNearlyZero());
}

void UPGCharacterAnimInstance::UpdateLocomotionAngle()
{
	LocalVelocityDirectionAngle = UKismetAnimationLibrary::CalculateDirection(
		OwningCharacter->GetVelocity(),
		OwningCharacter->GetActorRotation());
}

void UPGCharacterAnimInstance::UpdateLocomotionDirection()
{
	float AbsValue = FMath::Abs(LocalVelocityDirectionAngle);
	if (AbsValue <= 65.0f)
	{
		PoseWrappingEnum = EPGLocomotionDirection::Forward;
	}
	else if (AbsValue >= 115.0f)
	{
		PoseWrappingEnum = EPGLocomotionDirection::Back;
	}
	else if (LocalVelocityDirectionAngle >= 0)
	{
		PoseWrappingEnum = EPGLocomotionDirection::Right;
	}
	else
	{
		PoseWrappingEnum = EPGLocomotionDirection::Left;
	}

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

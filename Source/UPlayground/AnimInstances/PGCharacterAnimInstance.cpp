


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

	CurrentVelocity = OwningCharacter->GetVelocity().Size2D();

	bHasVelocity = (false == FVector2D(OwningCharacter->GetVelocity()).IsNearlyZero());
	bHasAcceleration = OwningMovementComponent->GetCurrentAcceleration().SizeSquared2D() > 0.f;

	LocomotionDirection = UKismetAnimationLibrary::CalculateDirection(
		OwningCharacter->GetVelocity(),
		OwningCharacter->GetActorRotation());


	// 위치 기반 속도 계산 (필요한 경우)
	const FVector NewWorldLocation = OwningCharacter->GetActorLocation();
	if (DeltaSeconds > 0.f)
	{
		DisplacementSinceLastUpdate = FVector::Dist2D(CurrentWorldLocation, NewWorldLocation);
		DisplacementSpeed = DisplacementSinceLastUpdate / DeltaSeconds;
	}
	else
	{
		DisplacementSinceLastUpdate = 0.f;
		DisplacementSpeed = 0.f;
	}
    
	// 다음 프레임을 위해 현재 위치 저장
	CurrentWorldLocation = NewWorldLocation;
}

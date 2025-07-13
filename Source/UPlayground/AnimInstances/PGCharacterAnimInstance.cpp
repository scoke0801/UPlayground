


#include "PGCharacterAnimInstance.h"
#include "AnimInstances/PGCharacterAnimInstance.h"

#include "KismetAnimationLibrary.h"
#include "Characters/LocalPlayer/PGLocalPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	GroundSpeed = OwningCharacter->GetVelocity().Size2D();

	bHasAcceleration = OwningMovementComponent->GetCurrentAcceleration().SizeSquared2D() > 0.f;

	LocomotionDirection = UKismetAnimationLibrary::CalculateDirection(
		OwningCharacter->GetVelocity(),
		OwningCharacter->GetActorRotation());
}

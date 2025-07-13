


#include "AnimInstances/LocalPlayer/PGPlayerAnimInstance.h"

#include "Characters/PGCharacterBase.h"
#include "Characters/LocalPlayer/PGLocalPlayerCharacter.h"

void UPGPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (nullptr != OwningCharacter)
	{
		OwningPlayerCharacter = Cast<APGLocalPlayerCharacter>(OwningCharacter);
	}
}

void UPGPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (bHasAcceleration)
	{
		IdleElapsedTime =0.f;
		bShouldEnterRelaxState = false;
	}
	else
	{
		IdleElapsedTime += DeltaSeconds;

		bShouldEnterRelaxState = (IdleElapsedTime >= EnterRelaxStateThreshold);
	}
}

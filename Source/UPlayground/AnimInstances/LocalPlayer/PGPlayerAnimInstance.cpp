


#include "AnimInstances/LocalPlayer/PGPlayerAnimInstance.h"

#include "PGActor/Characters/Player/PGCharacterPlayer.h"

void UPGPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (nullptr != OwningCharacter)
	{
		OwningPlayerCharacter = Cast<APGCharacterPlayer>(OwningCharacter);
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

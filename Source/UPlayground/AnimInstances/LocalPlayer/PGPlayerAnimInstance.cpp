


#include "AnimInstances/LocalPlayer/PGPlayerAnimInstance.h"

#include "EnhancedInputComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"

void UPGPlayerAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();
	
	if (nullptr != OwningCharacter)
	{
		if (APlayerController* PC = Cast<APlayerController>(OwningCharacter->GetController()))
		{
			OwningEIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
		}
	}
}

void UPGPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
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

void UPGPlayerAnimInstance::UpdateLocomotionDirection()
{
    // Quarter-view movement is camera-relative, while the attack faces the cursor.
    // Animation therefore needs actual velocity relative to the actor, not raw WASD axes.
    if (!OwningCharacter || Velocity.IsNearlyZero(.1f)) return;
    const FVector Local = OwningCharacter->GetActorRotation().UnrotateVector(Velocity);
    LocalVelocityDirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
    const float Angle = FMath::Abs(LocalVelocityDirectionAngle);
    PoseWrappingEnum = Angle <= 45.f ? EPGLocomotionDirection::Forward :
        Angle >= 135.f ? EPGLocomotionDirection::Back :
        LocalVelocityDirectionAngle > 0.f ? EPGLocomotionDirection::Right : EPGLocomotionDirection::Left;
}

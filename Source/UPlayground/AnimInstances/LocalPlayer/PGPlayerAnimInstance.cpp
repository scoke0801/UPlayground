


#include "AnimInstances/LocalPlayer/PGPlayerAnimInstance.h"

#include "EnhancedInputComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGData/DataAsset/Input/PGPlayerLocomotionData.h"
#include "Animation/AnimSequence.h"

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
    LocomotionTurnAnimation = nullptr;
    LocomotionTurnTime = LocomotionTurnWeight = 0.f;
}

void UPGPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    // Snapshot on the game thread; worker graph evaluation only reads these values.
    const auto* Player = Cast<APGCharacterPlayer>(TryGetPawnOwner());
    const auto* Data = Player ? Player->GetLocomotionData() : nullptr;
    if (!Player || !Data) { LocomotionTurnWeight = 0.f; return; }
    if (Player->GetLocomotionTurnAnimation())
        LocomotionTurnAnimation = Player->GetLocomotionTurnAnimation();
    LocomotionTurnTime = Player->GetLocomotionTurnTime();
    float TargetWeight = 0.f;
    if (Player->IsLocomotionTurning() && LocomotionTurnAnimation)
    {
        const float Remaining = (LocomotionTurnAnimation->GetPlayLength() - LocomotionTurnTime) / FMath::Max(.1f, Player->GetLocomotionTurnPlayRate());
        TargetWeight = FMath::Clamp(Remaining / FMath::Max(.01f, Data->BlendOutSeconds), 0.f, 1.f);
        const float Fraction = LocomotionTurnTime / FMath::Max(SMALL_NUMBER, LocomotionTurnAnimation->GetPlayLength());
        const float Resume = FMath::Clamp(Data->MovementResumeFraction, 0.f, .95f);
        TargetWeight *= 1.f - FMath::SmoothStep(Resume, FMath::Min(Resume + .25f, 1.f), Fraction);
    }
    const float BlendTime = TargetWeight > LocomotionTurnWeight ? Data->BlendInSeconds : Data->BlendOutSeconds;
    LocomotionTurnWeight = FMath::FInterpConstantTo(LocomotionTurnWeight, TargetWeight, DeltaSeconds, 1.f / FMath::Max(.01f, BlendTime));
}

void UPGPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	HumanoidGroundSpeed = Velocity.Size2D();
	bUseAirborneLocomotion = bIsJumping || bIsOnFalling;

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




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
	FVector2D MovementInput = OwningEIC->GetBoundActionValue(MoveInputAction).Get<FVector2D>();
                
	if (MovementInput.IsNearlyZero(0.1f))
	{
		return;
	}

	// 2D 입력을 3D 벡터로 변환
	FVector InputVector3D = FVector(MovementInput.X, MovementInput.Y, 0.0f);
	InputVector3D.Normalize();

	// 각도 계산
	LocalVelocityDirectionAngle = FMath::RadiansToDegrees(
		FMath::Atan2(InputVector3D.Y, InputVector3D.X)
	);

	// 방향 결정
	if (FMath::Abs(InputVector3D.X) > FMath::Abs(InputVector3D.Y))
	{
		PoseWrappingEnum = (InputVector3D.X > 0.0f) ? 
			EPGLocomotionDirection::Forward : EPGLocomotionDirection::Back;
	}
	else
	{
		PoseWrappingEnum = (InputVector3D.Y > 0.0f) ? 
			EPGLocomotionDirection::Right : EPGLocomotionDirection::Left;
	}
}

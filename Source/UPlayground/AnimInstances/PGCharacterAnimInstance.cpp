


#include "PGCharacterAnimInstance.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"

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
		
		if (APlayerController* PC = Cast<APlayerController>(OwningCharacter->GetController()))
		{
			OwningEIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
		}
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
		bIsJumping = Player->bIsJump;
	}
	
	bIsOnFalling = OwningMovementComponent->IsFalling();
}

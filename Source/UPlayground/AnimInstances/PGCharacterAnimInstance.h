

#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/PGBaseAnimInstance.h"
#include "PGCharacterAnimInstance.generated.h"

class UCharacterMovementComponent;
class APGCharacterBase;

UENUM(BlueprintType)
enum class EPGLocomotionDirection : uint8
{
	None = 0,
	Forward,
	Back,
	Left,
	Right
};
/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGCharacterAnimInstance : public UPGBaseAnimInstance
{
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	APGCharacterBase* OwningCharacter;

	UPROPERTY()
	UCharacterMovementComponent* OwningMovementComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	float CurrentVelocity;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	bool bHasAcceleration;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	bool bHasVelocity;

	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	FVector CurrentWorldLocation;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData", meta = (DisplayName = "Local Velocity Direction Angle"))
	float LocalVelocityDirectionAngle;
	
	// 지난 프레임에서부터 현재 프레임까지 이동거리
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData|DistanceMatching")
	float DisplacementSinceLastUpdate;
	
	// 지난 프레임에서부터 현재 프레임까지의 이동속도
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData|DistanceMatching")
	float DisplacementSpeed;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData|Pose Wrapping")
	EPGLocomotionDirection PoseWrappingEnum = EPGLocomotionDirection::None;
	
public:
	virtual void NativeInitializeAnimation() override;

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

private:
	void UpdateHasVelocity();
	void UpdateHasAcceleration();
	void UpdateLocomotionAngle();
	void UpdateLocomotionDirection();
	void UpdateDisplacementSpeed(float DeltaSeconds);
};



#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/PGBaseAnimInstance.h"
#include "PGCharacterAnimInstance.generated.h"

class UCharacterMovementComponent;
class APGCharacterBase;
/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGCharacterAnimInstance : public UPGBaseAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	
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
	float LocomotionDirection;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	FVector CurrentWorldLocation;

	// 지난 프레임에서부터 현재 프레임까지 이동거리
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData|DistanceMatching")
	float DisplacementSinceLastUpdate;
	
	// 지난 프레임에서부터 현재 프레임까지의 이동속도
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData|DistanceMatching")
	float DisplacementSpeed;

};

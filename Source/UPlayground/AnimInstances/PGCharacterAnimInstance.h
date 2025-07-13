

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
	float GroundSpeed;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	bool bHasAcceleration;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	float LocomotionDirection;
};

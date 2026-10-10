

#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/PGCharacterAnimInstance.h"
#include "PGPlayerAnimInstance.generated.h"

class APGCharacterPlayer;
/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGPlayerAnimInstance : public UPGCharacterAnimInstance
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient)
	class UEnhancedInputComponent* OwningEIC;
	
public:
	virtual void NativeBeginPlay() override;
	
	virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|References")
	APGCharacterPlayer* OwningPlayerCharacter;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	bool bShouldEnterRelaxState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	float EnterRelaxStateThreshold = 5.f;

	float IdleElapsedTime = 0.f;

	/** Actual movement speed for authored in-place locomotion, including hit-stop. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="AnimData|LocomotionData")
	float HumanoidGroundSpeed = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="AnimData|LocomotionData")
	bool bUseAirborneLocomotion = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category="AnimData|LocomotionData")
    TObjectPtr<class UAnimSequence> LocomotionTurnAnimation;
    UPROPERTY(Transient, BlueprintReadOnly, Category="AnimData|LocomotionData")
    float LocomotionTurnTime = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="AnimData|LocomotionData")
    float LocomotionTurnWeight = 0.f;

private:
	virtual void UpdateLocomotionDirection() override;
};

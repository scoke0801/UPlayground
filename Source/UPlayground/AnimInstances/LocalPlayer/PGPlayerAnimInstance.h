

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

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|References")
	APGCharacterPlayer* OwningPlayerCharacter;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	bool bShouldEnterRelaxState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "AnimData|LocomotionData")
	float EnterRelaxStateThreshold = 5.f;

	float IdleElapsedTime = 0.f;

private:
	virtual void UpdateLocomotionDirection() override;
};

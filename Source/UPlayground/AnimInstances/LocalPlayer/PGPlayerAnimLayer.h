

#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/PGBaseAnimInstance.h"
#include "PGPlayerAnimLayer.generated.h"

class UPGPlayerAnimInstance;
/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGPlayerAnimLayer : public UPGBaseAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe))
	UPGPlayerAnimInstance* GetPlayerAnimInstance() const;
};

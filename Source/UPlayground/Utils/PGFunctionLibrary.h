

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGFunctionLibrary.generated.h"

struct FGameplayTag;
class UPGPawnCombatComponent;
class UPGAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


public:
	UFUNCTION(BlueprintPure, Category = "PG|FunctionLibaray")
	static bool IsTargetPawnHostile(APawn* QueryPawn, APawn* TargetPawn);
	
};

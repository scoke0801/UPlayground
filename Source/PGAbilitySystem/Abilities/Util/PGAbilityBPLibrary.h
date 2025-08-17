// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGAbilityBPLibrary.generated.h"

class UPGPawnCombatComponent;
class UPGAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilityBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	
public:
	static UPGAbilitySystemComponent* NativeGetPGASCFromActor(AActor* InActor);

	static bool NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck);

public:
	UFUNCTION(BlueprintCallable, Category = "PG|FunctionLibaray")
	static void AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd);
	
	UFUNCTION(BlueprintCallable, Category = "PG|FunctionLibaray")
	static void RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove);

	UFUNCTION(BlueprintPure, Category = "PG|FunctionLibaray")
	static bool IsTargetPawnHostile(APawn* QueryPawn, APawn* TargetPawn);

	UFUNCTION(BlueprintPure, Category = "PG|FunctionLibaray", meta =(CompactNodeTitle = "Get Value At Level"))
	static float GetScalableFloatValueAtLevel(const FScalableFloat& InScalableFloat, float InLevel = 1.f);
	
	UFUNCTION(BlueprintPure, Category = "PG|FunctionLibaray")
	static bool IsValidBlock(AActor* InAttacker, AActor* InDefender);
	
};

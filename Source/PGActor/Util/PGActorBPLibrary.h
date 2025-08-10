// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGShared/Shared/Enum/PGEnumTypes.h"
#include "PGActorBPLibrary.generated.h"

class UPGPawnCombatComponent;
class UPGAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class PGACTOR_API UPGActorBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UPGPawnCombatComponent* NativeGetCombatComponentFromActor(AActor* InActor);
	
public:
	// ExpandEnumAsExecs: 지정된 enum의 각 값에 대응하는 출력 실행 핀들이 블루프린트 노드에 자동으로 생성
	UFUNCTION(BlueprintCallable, Category = "PG|FunctionLibaray", meta = (DisplayName = "Does Actor Have Tag", ExpandEnumAsExecs = "OutConfirmType"))
	static void BP_DoesActorHavTag(AActor* InActor, FGameplayTag TagToCheck, EPGConfirmType& OutConfirmType);

	UFUNCTION(BlueprintCallable, Category = "PG|FunctionLibaray", meta = (DisplayName = "Get Pawn Combat Component From Actor", ExpandEnumAsExecs = "OutValidType"))
	static UPGPawnCombatComponent* BP_GetPawnCombatComponentFromActor(AActor* InActor, EPGValidType& OutValidType);
	
};

#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGCombatTreeLibrary.generated.h"

class UBehaviorTree;
class UBlackboardData;

/** The same tree contract is used by runtime fallbacks and reproducible editor assets. */
UCLASS()
class PGAI_API UPGCombatTreeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    static void BuildTree(UBehaviorTree* Tree, UBlackboardData* Blackboard, float DecisionInterval);
    UFUNCTION(BlueprintCallable, Category="PG|AI|Authoring")
    static bool ConfigureEditorTree(UBehaviorTree* Tree, UBlackboardData* Blackboard, float DecisionInterval = .2f);
    UFUNCTION(BlueprintPure, Category="PG|AI|Authoring")
    static bool ValidateCombatTree(UBehaviorTree* Tree, bool bRequireEditorGraph = true);
};

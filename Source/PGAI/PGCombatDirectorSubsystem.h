#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PGCombatDirectorSubsystem.generated.h"

class APGCharacterEnemy;

/** Per-target attack admission. No tick and no world actor scans. */
UCLASS()
class PGAI_API UPGCombatDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
    friend class FPGCombatDirectorTest;
public:
    bool TryReserve(APGCharacterEnemy* Enemy, AActor* Target, int32 Cost);
    void Release(APGCharacterEnemy* Enemy);
    class UBehaviorTree* GetCombatBehaviorTree(float DecisionInterval);
    virtual void Deinitialize() override;
private:
    struct FRequest
    {
        TWeakObjectPtr<APGCharacterEnemy> Enemy;
        TWeakObjectPtr<AActor> Target;
        int32 Cost = 1;
        double LastSeen = 0;
    };
    TArray<FRequest> Active;
    // Insertion order prevents early-spawned enemies monopolizing every opening.
    TArray<FRequest> Waiting;
    TMap<TWeakObjectPtr<AActor>, double> NextStart;
    // One template per tuning interval per world, not one per spawned enemy.
    UPROPERTY(Transient) TMap<float, TObjectPtr<class UBehaviorTree>> CombatTrees;
    bool TryReserveAt(APGCharacterEnemy* Enemy, AActor* Target, int32 Cost, double Now, int32 Budget, float Spacing);
};

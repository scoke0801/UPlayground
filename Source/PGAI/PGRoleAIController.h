#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayAbilitySpecHandle.h"
#include "PGRoleAIController.generated.h"

/** Data-authored combat roles sharing BT execution, GAS attacks and director admission. */
UCLASS()
class PGAI_API APGRoleAIController : public AAIController
{
    GENERATED_BODY()
    friend class FPGBossSelectionTest;
    friend class FPGCombatBehaviorTreeTest;
public:
    APGRoleAIController();
    // Also used by isolated content QA without destroying the team-owning controller.
    UFUNCTION(BlueprintCallable, Category="PG|AI")
    void SetCombatThinkingEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="PG|AI")
    bool TryExecuteSkill(int32 SkillID);
    FGameplayAbilitySpecHandle GetAttackAbilityHandle();
    void ReleaseAttackReservation();
    void RefreshCombatContext();
    bool ShouldHoldPosition() const { return bHoldPosition; }
    bool TryRetreat();
    void ApproachTarget();
    UFUNCTION(BlueprintPure, Category="PG|AI")
    int32 GetPositionMoveCount() const { return PositionMoveCount; }
    UFUNCTION(BlueprintPure, Category="PG|AI")
    bool IsUsingCombatBehaviorTree() const { return bUsingCombatBehaviorTree; }
    UPROPERTY(EditDefaultsOnly, Category="PG|AI")
    bool bUseCombatBehaviorTree = true;
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use bUseCombatBehaviorTree"))
    bool bUseShooterBehaviorTree = true;
    UPROPERTY(EditDefaultsOnly, Category="PG|AI", meta=(ClampMin="0.05"))
    float DecisionInterval = .2f;
    UPROPERTY(EditDefaultsOnly, Category="PG|AI", meta=(ClampMin="0.1"))
    float MoveRetryInterval = .5f;
protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    int32 SelectSkill(const struct FPGEnemyDataRow& Data, const TArray<int32>& Candidates, int32 Phase,
        const TMap<int32, float>& Weights = {});
    int32 SequencePhase = 1;
    int32 SequenceCursor = 0;
    void Think();
    FTimerHandle ThinkTimer;
    int32 PreviousSkill = 0;
    int32 PendingSkill = 0;
    double RetreatUntil = 0;
    double NextRetreatAt = 0;
    double NextMoveRequestAt = 0;
    double NextPositionAt = 0;
    FVector PositionGoal = FVector::ZeroVector;
    bool bPositionMove = false;
    int32 PositionMoveCount = 0;
    bool TryPositionForCombat();
    float LastAcceptanceRadius = -1.f;
    float ApproachRange = 180.f;
    bool bHoldPosition = true;
    bool bCanSeeTarget = false;
    bool bUsingCombatBehaviorTree = false;
    TWeakObjectPtr<class APGCharacterPlayer> CombatTarget;
    TWeakObjectPtr<AActor> MoveTarget;
    UPROPERTY(Transient) TObjectPtr<class UBehaviorTree> CombatBehaviorTree;
    bool StartCombatBehaviorTree();
    void PublishCombatBlackboard();
};

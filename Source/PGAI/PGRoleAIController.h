#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "PGRoleAIController.generated.h"

/** Timer-driven combat roles for the authored arena. Legacy enemies retain their BT. */
UCLASS()
class PGAI_API APGRoleAIController : public AAIController
{
    GENERATED_BODY()
public:
    APGRoleAIController();
    // Also used by isolated content QA without destroying the team-owning controller.
    UFUNCTION(BlueprintCallable, Category="PG|AI")
    void SetCombatThinkingEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="PG|AI")
    bool TryExecuteSkill(int32 SkillID);
protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Think();
    FTimerHandle ThinkTimer;
    int32 PreviousSkill = 0;
    double NextDecision = 0;
    double RetreatUntil = 0;
    double NextRetreatAt = 0;
};

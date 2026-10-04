#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "PGBTService_CombatContext.generated.h"

UCLASS()
class PGAI_API UPGBTService_CombatContext : public UBTService
{
    GENERATED_BODY()
public:
    UPGBTService_CombatContext();
    void SetDecisionInterval(float Seconds) { Interval = FMath::Max(.05f, Seconds); }
protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

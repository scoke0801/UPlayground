#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "PGBTTask_CombatRole.generated.h"

UENUM()
enum class EPGCombatRoleAction : uint8 { Hold, Retreat, Approach };

/** Small actions used by the common role selector. Attack execution uses ExecuteSkill. */
UCLASS()
class PGAI_API UPGBTTask_CombatRole : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UPGBTTask_CombatRole();
    UPROPERTY(EditAnywhere, Category="PG|AI") EPGCombatRoleAction Action = EPGCombatRoleAction::Hold;
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

#include "PGBTTask_CombatRole.h"
#include "PGAI/PGRoleAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UPGBTTask_CombatRole::UPGBTTask_CombatRole() { NodeName = TEXT("Combat Role Action"); }
EBTNodeResult::Type UPGBTTask_CombatRole::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    auto* AI = Cast<APGRoleAIController>(OwnerComp.GetAIOwner());
    if (!AI) return EBTNodeResult::Failed;
    switch (Action)
    {
    case EPGCombatRoleAction::Hold: return AI->ShouldHoldPosition() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    case EPGCombatRoleAction::Retreat: return AI->TryRetreat() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    case EPGCombatRoleAction::Approach: AI->ApproachTarget(); return EBTNodeResult::Succeeded;
    default: return EBTNodeResult::Failed;
    }
}

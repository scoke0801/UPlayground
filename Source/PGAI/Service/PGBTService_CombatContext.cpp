#include "PGBTService_CombatContext.h"
#include "PGAI/PGRoleAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UPGBTService_CombatContext::UPGBTService_CombatContext()
{
    NodeName = TEXT("Update Combat Context");
    Interval = .2f;
    RandomDeviation = .02f;
    bCallTickOnSearchStart = true;
}
void UPGBTService_CombatContext::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    if (auto* AI = Cast<APGRoleAIController>(OwnerComp.GetAIOwner())) AI->RefreshCombatContext();
}

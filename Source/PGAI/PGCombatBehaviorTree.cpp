#include "PGCombatDirectorSubsystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Task/PGBTTask_CombatRole.h"
#include "Task/PGBTTask_ExecuteSkill.h"
#include "Service/PGBTService_CombatContext.h"

#include "PGCombatTreeLibrary.h"
#if WITH_EDITOR
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

UBehaviorTree* UPGCombatDirectorSubsystem::GetCombatBehaviorTree(float DecisionInterval)
{
    DecisionInterval = FMath::Max(.05f, DecisionInterval);
    if (auto* Existing = CombatTrees.Find(DecisionInterval)) return Existing->Get();
    auto* Tree = NewObject<UBehaviorTree>(this);
    auto* Board = NewObject<UBlackboardData>(Tree);
    UPGCombatTreeLibrary::BuildTree(Tree, Board, DecisionInterval);
    CombatTrees.Add(DecisionInterval, Tree);
    return Tree;
}

void UPGCombatTreeLibrary::BuildTree(UBehaviorTree* CombatBehaviorTree, UBlackboardData* BB, float DecisionInterval)
{
    check(CombatBehaviorTree && BB);
    BB->Keys.Reset();
    for (const FName Name : {FName("TargetActor"), FName("SkillTargetActor")})
    {
        FBlackboardEntry Entry; Entry.EntryName = Name;
        Entry.KeyType = NewObject<UBlackboardKeyType_Object>(BB);
        BB->Keys.Add(Entry);
    }
    for (const FName Name : {FName("SelectedSkillID"), FName("SummonCount")})
    {
        FBlackboardEntry Entry; Entry.EntryName = Name;
        Entry.KeyType = NewObject<UBlackboardKeyType_Int>(BB);
        BB->Keys.Add(Entry);
    }
    CombatBehaviorTree->BlackboardAsset = BB;
    auto* Root = NewObject<UBTComposite_Sequence>(CombatBehaviorTree);
    CombatBehaviorTree->RootNode = Root;
    auto* Context = NewObject<UPGBTService_CombatContext>(Root);
    Context->SetDecisionInterval(DecisionInterval);
    Root->Services.Add(Context);
    auto* Selector = NewObject<UBTComposite_Selector>(Root);
    FBTCompositeChild Branch; Branch.ChildComposite = Selector; Root->Children.Add(Branch);
    for (const EPGCombatRoleAction Action : {EPGCombatRoleAction::Hold, EPGCombatRoleAction::Retreat})
    {
        auto* Task = NewObject<UPGBTTask_CombatRole>(Selector); Task->Action = Action;
        Task->NodeName = Action == EPGCombatRoleAction::Hold ? TEXT("Hold / Recovery / No Target") : TEXT("Limited Retreat");
        FBTCompositeChild Child; Child.ChildTask = Task; Selector->Children.Add(Child);
    }
    FBTCompositeChild Attack; Attack.ChildTask = NewObject<UPGBTTask_ExecuteSkill>(Selector); Selector->Children.Add(Attack);
    auto* Approach = NewObject<UPGBTTask_CombatRole>(Selector); Approach->Action = EPGCombatRoleAction::Approach;
    Approach->NodeName = TEXT("Approach / Wait For Attack Budget");
    FBTCompositeChild Move; Move.ChildTask = Approach; Selector->Children.Add(Move);
    auto* Wait = NewObject<UBTTask_Wait>(Root); Wait->WaitTime = FMath::Max(.05f, DecisionInterval); Wait->RandomDeviation = .02f;
    FBTCompositeChild Pause; Pause.ChildTask = Wait; Root->Children.Add(Pause);
}

bool UPGCombatTreeLibrary::ConfigureEditorTree(UBehaviorTree* Tree, UBlackboardData* Blackboard, float DecisionInterval)
{
#if WITH_EDITOR
    if (!Tree || !Blackboard || !FMath::IsFinite(DecisionInterval) || DecisionInterval < .05f) return false;
    Tree->Modify(); Blackboard->Modify();
    BuildTree(Tree, Blackboard, DecisionInterval);
    if (Tree->BTGraph) Tree->BTGraph->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    const auto SchemaClass = GetDefault<UBehaviorTreeGraph>()->Schema;
    Tree->BTGraph = FBlueprintEditorUtils::CreateNewGraph(Tree, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), SchemaClass);
    auto* Graph = CastChecked<UBehaviorTreeGraph>(Tree->BTGraph);
    Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
    Graph->OnCreated(); Graph->Initialize();
    // AutoArrange requires live Slate node widgets and crashes in commandlets.
    // Place this small, fixed topology directly so the same generator is headless-safe.
    for (const auto& NodePtr : Graph->Nodes)
    {
        auto* Node = NodePtr.Get();
        auto* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
        if (!BTNode) continue;
        auto* Instance = BTNode->NodeInstance.Get();
        if (!Instance) { Node->NodePosX = 600; Node->NodePosY = 0; }
        else if (Instance == Tree->RootNode) { Node->NodePosX = 600; Node->NodePosY = 180; }
        else if (Instance->IsA<UBTComposite_Selector>()) { Node->NodePosX = 500; Node->NodePosY = 420; }
        else if (Instance->IsA<UBTTask_Wait>()) { Node->NodePosX = 1550; Node->NodePosY = 420; }
        else if (Instance->IsA<UPGBTTask_ExecuteSkill>()) { Node->NodePosX = 800; Node->NodePosY = 660; }
        else if (auto* Role = Cast<UPGBTTask_CombatRole>(Instance))
        {
            Node->NodePosX = Role->Action == EPGCombatRoleAction::Hold ? 0 : Role->Action == EPGCombatRoleAction::Retreat ? 400 : 1200;
            Node->NodePosY = 660;
        }
    }
    Graph->OnSave();
    Tree->MarkPackageDirty(); Blackboard->MarkPackageDirty();
    return ValidateCombatTree(Tree, true);
#else
    return false;
#endif
}

bool UPGCombatTreeLibrary::ValidateCombatTree(UBehaviorTree* Tree, bool bRequireEditorGraph)
{
    if (!Tree || !Tree->BlackboardAsset || !Tree->RootNode || Tree->RootNode->Children.Num() != 2) return false;
    for (const FName Name : {FName("TargetActor"), FName("SelectedSkillID"), FName("SkillTargetActor"), FName("SummonCount")})
        if (Tree->BlackboardAsset->GetKeyID(Name) == FBlackboard::InvalidKey) return false;
    const auto* Selector = Tree->RootNode->Children[0].ChildComposite.Get();
    if (!Selector || Selector->Children.Num() != 4 || !Selector->Children[2].ChildTask ||
        !Selector->Children[2].ChildTask->IsA<UPGBTTask_ExecuteSkill>()) return false;
#if WITH_EDITOR
    if (bRequireEditorGraph && (!Tree->BTGraph || Tree->BTGraph->Nodes.Num() < 7)) return false;
#endif
    return true;
}

#include "PGHumanoidLocomotionTools.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

bool UPGHumanoidLocomotionTools::RemoveRetargetedTurnYaw(UAnimSequence* Animation, const TArray<float>& RotationProgress, float Angle)
{
    if (!Animation || RotationProgress.Num() < 2) return false;
    const FReferenceSkeleton& Skeleton = Animation->GetSkeleton()->GetReferenceSkeleton();
    const int32 Pelvis = Skeleton.FindBoneIndex(TEXT("pelvis"));
    if (Pelvis == INDEX_NONE || Skeleton.GetParentIndex(Pelvis) != 0) return false;
    // The pelvis retarget operation absorbs source root yaw into pelvis. Bake that yaw
    // out of the generated copy before the capsule consumes the same rotation curve.
    TArray<FTransform> Keys;
    Animation->GetDataModel()->GetBoneTrackTransforms(TEXT("pelvis"), Keys);
    if (Keys.Num() < 2) return false;
    TArray<FVector> Positions, Scales;
    TArray<FQuat> Rotations;
    for (int32 Frame = 0; Frame < Keys.Num(); ++Frame)
    {
        const float Sample = float(Frame) / (Keys.Num() - 1) * (RotationProgress.Num() - 1);
        const int32 Index = FMath::Min(FMath::FloorToInt(Sample), RotationProgress.Num() - 2);
        const float Yaw = Angle * FMath::Lerp(RotationProgress[Index], RotationProgress[Index + 1], Sample - Index);
        const FQuat Undo(FVector::UpVector, FMath::DegreesToRadians(-Yaw));
        Positions.Add(Undo.RotateVector(Keys[Frame].GetTranslation()));
        Rotations.Add((Undo * Keys[Frame].GetRotation()).GetNormalized());
        Scales.Add(Keys[Frame].GetScale3D());
    }
    Animation->Modify();
    auto& Controller = Animation->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Remove capsule-owned turn rotation")), false);
    const bool Result = Controller.SetBoneTrackKeys(TEXT("pelvis"), Positions, Rotations, Scales, false);
    Controller.CloseBracket(false);
    Animation->MarkPackageDirty();
    return Result;
}

bool UPGHumanoidLocomotionTools::ConfigurePlayerTurnLocomotion(UAnimBlueprint* Blueprint, UAnimSequence* DefaultTurn)
{
    if (!Blueprint || !DefaultTurn || Blueprint->TargetSkeleton != DefaultTurn->GetSkeleton()) return false;
    UEdGraph* Graph = nullptr;
    for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
        if (Candidate->GetFName() == TEXT("AnimGraph")) Graph = Candidate;
    if (!Graph) return false;
    // Reauthor only our own three input nodes, evaluator and blend.
    for (int32 Index = Graph->Nodes.Num() - 1; Index >= 0; --Index)
        if (Graph->Nodes[Index]->GetName().StartsWith(TEXT("PGPlayerTurn")))
            FBlueprintEditorUtils::RemoveNode(Blueprint, Graph->Nodes[Index], true);
    UAnimGraphNode_BlendSpacePlayer* Ground = nullptr;
    UAnimGraphNode_BlendListByBool* Air = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->GetFName() == TEXT("PGHumanoidGround")) Ground = Cast<UAnimGraphNode_BlendSpacePlayer>(Node);
        if (Node->GetFName() == TEXT("PGHumanoidAirborne")) Air = Cast<UAnimGraphNode_BlendListByBool>(Node);
    }
    if (!Ground || !Air) return false;
    Blueprint->Modify(); Graph->Modify();
    auto* Turn = NewObject<UAnimGraphNode_SequenceEvaluator>(Graph, TEXT("PGPlayerTurnEvaluator"), RF_Transactional);
    Graph->AddNode(Turn, false, false); Turn->CreateNewGuid(); Turn->SetAnimationAsset(DefaultTurn);
    Turn->PostPlacedNewNode(); Turn->AllocateDefaultPins();
    for (auto& Pin : Turn->ShowPinForProperties)
        if (Pin.PropertyName == TEXT("Sequence")) Pin.bShowPin = true;
    Turn->ReconstructNode();
    Turn->Node.SetShouldLoop(false);
    Turn->Node.SetTeleportToExplicitTime(true);
    auto* Blend = NewObject<UAnimGraphNode_TwoWayBlend>(Graph, TEXT("PGPlayerTurnBlend"), RF_Transactional);
    Graph->AddNode(Blend, false, false); Blend->CreateNewGuid(); Blend->PostPlacedNewNode(); Blend->AllocateDefaultPins();
    Turn->NodePosX = Ground->NodePosX; Turn->NodePosY = Ground->NodePosY + 350;
    Blend->NodePosX = Air->NodePosX; Blend->NodePosY = Ground->NodePosY + 250;
    const UEdGraphSchema* Schema = Graph->GetSchema();
    auto Connect = [Schema](UEdGraphPin* A, UEdGraphPin* B) { return A && B && Schema->TryCreateConnection(A, B); };
    auto Variable = [Graph, Turn](FName Name, int32 Offset)
    {
        auto* Node = NewObject<UK2Node_VariableGet>(Graph, *FString::Printf(TEXT("PGPlayerTurnInput%d"), Offset), RF_Transactional);
        Graph->AddNode(Node, false, false); Node->CreateNewGuid();
        Node->VariableReference.SetSelfMember(Name); Node->AllocateDefaultPins();
        Node->NodePosX = Turn->NodePosX - 300; Node->NodePosY = Turn->NodePosY + Offset;
        return Node->FindPin(Name, EGPD_Output);
    };
    if (!Connect(Variable(TEXT("LocomotionTurnAnimation"), 0), Turn->FindPin(TEXT("Sequence"))) ||
        !Connect(Variable(TEXT("LocomotionTurnTime"), 100), Turn->FindPin(TEXT("ExplicitTime"))) ||
        !Connect(Variable(TEXT("LocomotionTurnWeight"), 200), Blend->FindPin(TEXT("Alpha"))) ||
        !Connect(Ground->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("A"))) ||
        !Connect(Turn->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("B")))) return false;
    Air->FindPin(TEXT("BlendPose_1"))->BreakAllPinLinks();
    if (!Connect(Blend->FindPin(TEXT("Pose")), Air->FindPin(TEXT("BlendPose_1")))) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
}

void UPGHumanoidLocomotionTools::RebuildBlendSpace(UBlendSpace* BlendSpace)
{
    if (!BlendSpace) return;
    BlendSpace->Modify();
    BlendSpace->ResampleData();
    BlendSpace->MarkPackageDirty();
}

int32 UPGHumanoidLocomotionTools::GetBlendSampleCount(UBlendSpace* BlendSpace, FVector Input)
{
    if (!BlendSpace) return 0;
    TArray<FBlendSampleData> Samples;
    int32 Index = INDEX_NONE;
    return BlendSpace->GetSamplesFromBlendInput(Input, Samples, Index, true) ? Samples.Num() : 0;
}

bool UPGHumanoidLocomotionTools::ConfigurePlayerGroundLocomotion(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace)
{
    if (!Blueprint || !BlendSpace || Blueprint->TargetSkeleton != BlendSpace->GetSkeleton()) return false;
    UEdGraph* Graph = nullptr;
    for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
        if (Candidate->GetFName() == TEXT("AnimGraph")) Graph = Candidate;
    if (!Graph) return false;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (Node->GetFName() == TEXT("PGHumanoidGround"))
        {
            auto* Existing = Cast<UAnimGraphNode_BlendSpacePlayer>(Node);
            if (!Existing) return false;
            Existing->SetAnimationAsset(BlendSpace);
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            return Blueprint->Status != BS_Error;
        }
    UEdGraphPin* Target = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (Node->GetClass()->GetFName() == TEXT("AnimGraphNode_ControlRig"))
            for (UEdGraphPin* Pin : Node->Pins)
                if (Pin->Direction == EGPD_Input && Pin->PinName == TEXT("Source")) Target = Pin;
    if (!Target || Target->LinkedTo.Num() != 1) return false;
    UEdGraphPin* Original = Target->LinkedTo[0];
    Blueprint->Modify(); Graph->Modify();
    auto* Ground = NewObject<UAnimGraphNode_BlendSpacePlayer>(Graph, TEXT("PGHumanoidGround"), RF_Transactional);
    Graph->AddNode(Ground, false, false); Ground->CreateNewGuid();
    Ground->SetAnimationAsset(BlendSpace); Ground->PostPlacedNewNode(); Ground->AllocateDefaultPins();
    Ground->NodePosX = Target->GetOwningNode()->NodePosX - 550;
    Ground->NodePosY = Target->GetOwningNode()->NodePosY + 450;
    auto* Air = NewObject<UAnimGraphNode_BlendListByBool>(Graph, TEXT("PGHumanoidAirborne"), RF_Transactional);
    Graph->AddNode(Air, false, false); Air->CreateNewGuid(); Air->PostPlacedNewNode(); Air->AllocateDefaultPins();
    Air->NodePosX = Ground->NodePosX + 300; Air->NodePosY = Ground->NodePosY;
    // Keep jump transitions advancing while grounded. Existing montage/IK nodes remain downstream.
    FProperty* Mode = FAnimNode_BlendListBase::StaticStruct()->FindPropertyByName(TEXT("ChildUpateMode"));
    if (!Mode) return false;
    Mode->ImportText_Direct(TEXT("AlwaysTickChildren"), Mode->ContainerPtrToValuePtr<void>(&Air->Node), nullptr, PPF_None);
    const UEdGraphSchema* Schema = Graph->GetSchema();
    auto Connect = [Schema](UEdGraphPin* A, UEdGraphPin* B) { return A && B && Schema->TryCreateConnection(A, B); };
    auto Variable = [Graph, Ground](FName Name, int32 Offset)
    {
        auto* Node = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
        Graph->AddNode(Node, false, false); Node->CreateNewGuid();
        Node->VariableReference.SetSelfMember(Name); Node->AllocateDefaultPins();
        Node->NodePosX = Ground->NodePosX - 250; Node->NodePosY = Ground->NodePosY + Offset;
        return Node->FindPin(Name, EGPD_Output);
    };
    if (!Connect(Variable(TEXT("LocalVelocityDirectionAngle"), 0), Ground->FindPin(TEXT("X"))) ||
        !Connect(Variable(TEXT("HumanoidGroundSpeed"), 100), Ground->FindPin(TEXT("Y"))) ||
        !Connect(Variable(TEXT("bUseAirborneLocomotion"), 200), Air->FindPin(TEXT("bActiveValue"))) ||
        !Connect(Original, Air->FindPin(TEXT("BlendPose_0"))) ||
        !Connect(Ground->FindPin(TEXT("Pose")), Air->FindPin(TEXT("BlendPose_1")))) return false;
    Target->BreakAllPinLinks();
    if (!Connect(Air->FindPin(TEXT("Pose")), Target)) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
}

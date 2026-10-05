#include "PGAppearanceAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

struct FPGAppearanceRetargetNode : FAnimNode_RetargetPoseFromMesh
{
    TArray<FVector> ReferenceScales;
    TArray<int32> Parents;
    bool bHasScaledRoot = false;
    bool bReconstructScaledTranslations = false;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
    {
        FAnimNode_RetargetPoseFromMesh::CacheBones_AnyThread(Context);
        const auto& Ref = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetSkeletalMeshAsset()->GetRefSkeleton();
        ReferenceScales.SetNum(Ref.GetNum());
        Parents.SetNum(Ref.GetNum());
        bHasScaledRoot = Ref.GetNum() && !Ref.GetRefBonePose()[0].GetScale3D().Equals(FVector::OneVector, .01);
        for (int32 Bone=0; Bone<Ref.GetNum(); ++Bone)
        {
            Parents[Bone] = Ref.GetParentIndex(Bone);
            ReferenceScales[Bone] = Ref.GetRefBonePose()[Bone].GetScale3D();
            if (Parents[Bone] != INDEX_NONE) ReferenceScales[Bone] *= ReferenceScales[Parents[Bone]];
        }
    }
    virtual void Evaluate_AnyThread(FPoseContext& Output) override
    {
        FAnimNode_RetargetPoseFromMesh::Evaluate_AnyThread(Output);
        auto* RetargetProcessor = GetRetargetProcessor();
        if (!bReconstructScaledTranslations || !bHasScaledRoot || !RetargetProcessor->IsInitialized()) return;
        // Pelvis motion is in centimeters, while FK descendants retain imported local
        // units. Normalize only the root-to-pelvis path under the restored parent scale.
        // Scaling the FK descendants again would collapse the body around the pelvis.
        const int32 Pelvis = RetargetProcessor->GetTargetSkeleton().FindBoneIndexByName(
            RetargetProcessor->GetPelvisBone(ERetargetSourceOrTarget::Target, ERetargetOpsToSearch::AssetOps));
        const auto& Required = Output.AnimInstanceProxy->GetRequiredBones().GetBoneIndicesArray();
        for (int32 Compact=0; Compact<Required.Num(); ++Compact)
        {
            const int32 Bone = Required[Compact], Parent = Parents[Bone];
            for (int32 Ancestor=Pelvis; Ancestor!=INDEX_NONE; Ancestor=Parents[Ancestor])
            {
                if (Ancestor != Bone || Parent == INDEX_NONE) continue;
                auto& Local = Output.Pose[FCompactPoseBoneIndex(Compact)];
                Local.SetTranslation(Local.GetTranslation() * FTransform::GetSafeScaleReciprocal(ReferenceScales[Parent]));
                break;
            }
        }
    }
};

struct FPGAppearanceAnimProxy : FAnimInstanceProxy
{
    explicit FPGAppearanceAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    FPGAppearanceRetargetNode RetargetNode;
    virtual FAnimNode_Base* GetCustomRootNode() override { return &RetargetNode; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes.Add(&RetargetNode); }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        RetargetNode.IKRetargeterAsset = CastChecked<UPGAppearanceAnimInstance>(Instance)->Retargeter;
        RetargetNode.bReconstructScaledTranslations = CastChecked<UPGAppearanceAnimInstance>(Instance)->bReconstructScaledTranslations;
        FAnimInstanceProxy::Initialize(Instance);
    }
    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        RetargetNode.IKRetargeterAsset = CastChecked<UPGAppearanceAnimInstance>(Instance)->Retargeter;
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
    }
};

FAnimInstanceProxy* UPGAppearanceAnimInstance::CreateAnimInstanceProxy() { return new FPGAppearanceAnimProxy(this); }
void UPGAppearanceAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }

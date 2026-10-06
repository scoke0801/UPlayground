#include "PGAppearanceAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PGCharacterAppearanceComponent.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"

struct FPGAppearanceRetargetNode : FAnimNode_RetargetPoseFromMesh
{
    TArray<FVector> ReferenceScales;
    TArray<int32> Parents;
    bool bHasScaledRoot = false;
    bool bReconstructScaledTranslations = false;
    TArray<FPGAppearanceGripProfile> GripProfiles;
    struct FFinger { FCompactPoseBoneIndex Index; FQuat Rotation; };
    struct FGrip { TArray<FFinger> Fingers; float Weight = 0.f; };
    TArray<FGrip> Grips;
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
        Grips.SetNum(GripProfiles.Num());
        const auto& Required = Context.AnimInstanceProxy->GetRequiredBones();
        for (int32 I = 0; I < GripProfiles.Num(); ++I)
        {
            auto& Cache = Grips[I]; Cache.Fingers.Reset();
            const auto& Profile = GripProfiles[I];
            const int32 Hand = Ref.FindBoneIndex(Profile.TargetSocket);
            TSet<int32> Seen;
            for (const auto& Finger : Profile.Fingers)
            {
                const int32 Bone = Ref.FindBoneIndex(Finger.Bone);
                if (Hand == INDEX_NONE || Bone == INDEX_NONE || Bone == Hand || !Ref.BoneIsChildOf(Bone, Hand) ||
                    Finger.ReferenceRotationOffset.ContainsNaN() || Seen.Contains(Bone))
                { Cache.Fingers.Reset(); break; }
                Seen.Add(Bone);
                const auto Compact = Required.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bone));
                if (Compact.GetInt() == INDEX_NONE) continue; // Rebuilt when LOD required bones change.
                Cache.Fingers.Add({Compact, (Ref.GetRefBonePose()[Bone].GetRotation() * Finger.ReferenceRotationOffset.Quaternion()).GetNormalized()});
            }
        }
    }
    virtual void Evaluate_AnyThread(FPoseContext& Output) override
    {
        FAnimNode_RetargetPoseFromMesh::Evaluate_AnyThread(Output);
        auto* RetargetProcessor = GetRetargetProcessor();
        if (!RetargetProcessor->IsInitialized()) return;
        // Pelvis motion is in centimeters, while FK descendants retain imported local
        // units. Normalize only the root-to-pelvis path under the restored parent scale.
        // Scaling the FK descendants again would collapse the body around the pelvis.
        if (bReconstructScaledTranslations && bHasScaledRoot)
        {
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
        // Snapshot-only finger evaluation: no owner, equipment or world access on workers.
        for (const auto& Grip : Grips)
            if (Grip.Weight > UE_SMALL_NUMBER)
                for (const auto& Finger : Grip.Fingers)
                {
                    auto& Local = Output.Pose[Finger.Index];
                    Local.SetRotation(FQuat::Slerp(Local.GetRotation(), Finger.Rotation, Grip.Weight).GetNormalized());
                }
    }
};

struct FPGAppearanceAnimProxy : FAnimInstanceProxy
{
    explicit FPGAppearanceAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    FPGAppearanceRetargetNode RetargetNode;
    int32 EquipmentGripCount = 0;
    virtual FAnimNode_Base* GetCustomRootNode() override { return &RetargetNode; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes.Add(&RetargetNode); }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        RetargetNode.IKRetargeterAsset = CastChecked<UPGAppearanceAnimInstance>(Instance)->Retargeter;
        RetargetNode.bReconstructScaledTranslations = CastChecked<UPGAppearanceAnimInstance>(Instance)->bReconstructScaledTranslations;
        const auto* Appearance = CastChecked<UPGAppearanceAnimInstance>(Instance)->Appearance.Get();
        RetargetNode.GripProfiles = Appearance ? Appearance->GripProfiles : TArray<FPGAppearanceGripProfile>();
        EquipmentGripCount = RetargetNode.GripProfiles.Num();
        if (Appearance)
            for (const auto& Attachment : Appearance->Attachments)
                if (!Attachment.Fingers.IsEmpty())
                {
                    FPGAppearanceGripProfile Grip;
                    Grip.TargetSocket = Attachment.AttachBone;
                    Grip.Fingers = Attachment.Fingers;
                    RetargetNode.GripProfiles.Add(MoveTemp(Grip));
                }
        RetargetNode.Grips.Reset();
        FAnimInstanceProxy::Initialize(Instance);
    }
    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        RetargetNode.IKRetargeterAsset = CastChecked<UPGAppearanceAnimInstance>(Instance)->Retargeter;
        const auto* Owner = Instance->GetOwningActor();
        const auto* Appearance = Owner ? Owner->FindComponentByClass<UPGCharacterAppearanceComponent>() : nullptr;
        const int32 Active = Appearance ? Appearance->GetEquippedGripIndex() : INDEX_NONE;
        for (int32 I = 0; I < RetargetNode.Grips.Num(); ++I)
            RetargetNode.Grips[I].Weight = FMath::FInterpConstantTo(RetargetNode.Grips[I].Weight,
                I >= EquipmentGripCount || I == Active ? 1.f : 0.f, DeltaSeconds, 1.f / .12f);
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
    }
};

FAnimInstanceProxy* UPGAppearanceAnimInstance::CreateAnimInstanceProxy() { return new FPGAppearanceAnimProxy(this); }
void UPGAppearanceAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }

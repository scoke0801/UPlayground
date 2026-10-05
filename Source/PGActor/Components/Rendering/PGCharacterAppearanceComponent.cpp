#include "PGCharacterAppearanceComponent.h"
#include "PGAppearanceAnimInstance.h"
#include "PGToonPresentationComponent.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Retargeter/IKRetargeter.h"

UPGCharacterAppearanceComponent::UPGCharacterAppearanceComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UPGCharacterAppearanceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!DefaultAppearance.IsNull() && !ApplyAppearance(DefaultAppearance.LoadSynchronous()))
        UE_LOG(LogTemp, Error, TEXT("Appearance initialization failed for %s"), *GetNameSafe(GetOwner()));
}

bool UPGCharacterAppearanceComponent::CanApply(UPGCharacterAppearance* Appearance) const
{
    const auto* Character = Cast<ACharacter>(GetOwner());
    auto* Source = Character ? Character->GetMesh()->GetSkeletalMeshAsset() : nullptr;
    if (!Appearance || !Source) return false;
    auto* ExpectedSource = Appearance->SourceMesh.LoadSynchronous();
    auto* Mesh = Appearance->Mesh.LoadSynchronous();
    if (!ExpectedSource || Source->GetSkeleton() != ExpectedSource->GetSkeleton() || !Mesh ||
        !Cast<UIKRetargeter>(Appearance->Retargeter.TryLoad()) || !Appearance->MeshTransform.IsValid()) return false;
    for (const auto& Part : Appearance->Parts)
    {
        auto* PartMesh = Part.Mesh.LoadSynchronous();
        if (!PartMesh || !Part.RelativeTransform.IsValid()) return false;
        if (Part.AttachBone.IsNone() && PartMesh->GetSkeleton() != Mesh->GetSkeleton()) return false;
        if (!Part.AttachBone.IsNone() && Mesh->GetRefSkeleton().FindBoneIndex(Part.AttachBone) == INDEX_NONE) return false;
    }
    return true;
}

void UPGCharacterAppearanceComponent::AddToon(USkeletalMeshComponent* Mesh, UPGCharacterAppearance* Appearance)
{
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetRenderCustomDepth(true);
    Mesh->SetCustomDepthStencilValue(73);
    Mesh->bReceivesDecals = false;
    auto* Toon = NewObject<UPGToonPresentationComponent>(GetOwner());
    Toon->HeadBone = Appearance->HeadBone;
    Toon->HeadForwardAxis = Appearance->HeadForwardAxis;
    Toon->HeadRightAxis = Appearance->HeadRightAxis;
    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
        if (!It->IsHidden()) { Toon->KeyLight = *It; break; }
    Toon->RegisterComponent();
    Toon->Initialize(Mesh);
    Presentations.Add(Toon);
}

bool UPGCharacterAppearanceComponent::ApplyAppearance(UPGCharacterAppearance* Appearance)
{
    if (CurrentAppearance == Appearance && VisibleMesh) return true;
    if (!CanApply(Appearance)) return false;
    auto* Character = CastChecked<ACharacter>(GetOwner());
    auto* Source = Character->GetMesh();
    // Return carried equipment to its original socket before destroying old anchors.
    TArray<AActor*> Attached;
    Character->GetAttachedActors(Attached);
    for (auto* Actor : Attached)
        if (auto* Root = Actor->GetRootComponent())
            for (const auto& Pair : EquipmentAnchors)
                if (Root->GetAttachParent() == Pair.Value)
                { Actor->AttachToComponent(Source, FAttachmentTransformRules::KeepRelativeTransform, Pair.Key); break; }
    ClearPresentation();
    // Keep the existing AnimBP, montage notifies, hit-stop and GAS authority intact.
    TArray<USkeletalMeshComponent*> ExistingMeshes;
    Character->GetComponents(ExistingMeshes);
    for (auto* Mesh : ExistingMeshes) { Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true); Mesh->SetCastShadow(false); }
    Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Source->bEnableUpdateRateOptimizations = false;
    VisibleMesh = NewObject<USkeletalMeshComponent>(Character, MakeUniqueObjectName(Character, USkeletalMeshComponent::StaticClass(), TEXT("PGAppearanceMesh")));
    VisibleMesh->SetupAttachment(Source);
    VisibleMesh->SetRelativeTransform(Appearance->MeshTransform);
    VisibleMesh->SetSkeletalMesh(Appearance->Mesh.LoadSynchronous());
    VisibleMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    VisibleMesh->AddTickPrerequisiteComponent(Source);
    VisibleMesh->RegisterComponent();
    VisibleMesh->SetAnimInstanceClass(UPGAppearanceAnimInstance::StaticClass());
    auto* Anim = CastChecked<UPGAppearanceAnimInstance>(VisibleMesh->GetAnimInstance());
    Anim->Retargeter = CastChecked<UIKRetargeter>(Appearance->Retargeter.ResolveObject());
    Anim->bReconstructScaledTranslations = Appearance->bReconstructScaledTranslations;
    // Initialize only after assigning the retarget asset.
    Anim->InitializeAnimation();
    AddToon(VisibleMesh, Appearance);
    for (const auto& Definition : Appearance->Parts)
    {
        auto* Part = NewObject<USkeletalMeshComponent>(Character);
        Part->SetupAttachment(VisibleMesh, Definition.AttachBone);
        Part->SetRelativeTransform(Definition.RelativeTransform);
        Part->SetSkeletalMesh(Definition.Mesh.LoadSynchronous());
        Part->RegisterComponent();
        if (Definition.AttachBone.IsNone()) Part->SetLeaderPoseComponent(VisibleMesh);
        AddToon(Part, Appearance);
        Parts.Add(Part);
    }
    CurrentAppearance = Appearance;
    for (auto* Actor : Attached)
        if (auto* Root = Actor->GetRootComponent(); Root && Root->GetAttachParent() == Source)
        {
            FName Socket = Root->GetAttachSocketName();
            auto* Attachment = ResolveEquipmentAttachment(Socket);
            Actor->AttachToComponent(Attachment, FAttachmentTransformRules::KeepRelativeTransform, Socket);
        }
    return true;
}

USceneComponent* UPGCharacterAppearanceComponent::ResolveEquipmentAttachment(FName& Socket)
{
    auto* Character = Cast<ACharacter>(GetOwner());
    auto* Source = Character ? Character->GetMesh() : nullptr;
    if (!Source || !VisibleMesh || !CurrentAppearance || Socket.IsNone()) return Source;
    if (const auto* Existing = EquipmentAnchors.Find(Socket)) { Socket = NAME_None; return Existing->Get(); }
    const auto* SourceAsset = Source->GetSkeletalMeshAsset();
    const auto* TargetAsset = VisibleMesh->GetSkeletalMeshAsset();
    const FReferenceSkeleton& SourceRef = SourceAsset->GetRefSkeleton();
    const FReferenceSkeleton& TargetRef = TargetAsset->GetRefSkeleton();
    const auto* SocketAsset = SourceAsset->FindSocket(Socket);
    const FName Bone = SocketAsset ? SocketAsset->BoneName : Socket;
    int32 Index = SourceRef.FindBoneIndex(Bone);
    if (Index == INDEX_NONE) return Source;
    const auto RefTransform = [](const FReferenceSkeleton& Ref, int32 BoneIndex)
    {
        FTransform Result = Ref.GetRefBonePose()[BoneIndex];
        while ((BoneIndex = Ref.GetParentIndex(BoneIndex)) != INDEX_NONE) Result *= Ref.GetRefBonePose()[BoneIndex];
        return Result;
    };
    FTransform SocketRef = (SocketAsset ? SocketAsset->GetSocketLocalTransform() : FTransform::Identity) * RefTransform(SourceRef, Index);
    const FName* TargetBone = nullptr;
    while (Index != INDEX_NONE)
    {
        TargetBone = CurrentAppearance->EquipmentBones.Find(SourceRef.GetBoneName(Index));
        if (TargetBone) break;
        Index = SourceRef.GetParentIndex(Index);
    }
    const int32 TargetIndex = TargetBone ? TargetRef.FindBoneIndex(*TargetBone) : INDEX_NONE;
    if (TargetIndex == INDEX_NONE) return Source;
    const FTransform SourceBoneRef = RefTransform(SourceRef, Index);
    const FTransform TargetBoneRef = RefTransform(TargetRef, TargetIndex);
    SocketRef.SetTranslation(TargetBoneRef.GetTranslation() + SocketRef.GetTranslation() - SourceBoneRef.GetTranslation());
    auto* Anchor = NewObject<USceneComponent>(Character);
    Anchor->SetupAttachment(VisibleMesh, *TargetBone);
    Anchor->SetRelativeTransform(SocketRef.GetRelativeTransform(TargetBoneRef));
    Anchor->RegisterComponent();
    EquipmentAnchors.Add(Socket, Anchor);
    Socket = NAME_None;
    return Anchor;
}

void UPGCharacterAppearanceComponent::ClearPresentation()
{
    for (const auto& Pair : EquipmentAnchors) if (Pair.Value) Pair.Value->DestroyComponent();
    EquipmentAnchors.Reset();
    for (UPGToonPresentationComponent* Toon : Presentations) if (Toon) { Toon->Initialize(nullptr); Toon->DestroyComponent(); }
    Presentations.Reset();
    for (USkeletalMeshComponent* Part : Parts) if (Part) Part->DestroyComponent();
    Parts.Reset();
    if (VisibleMesh) VisibleMesh->DestroyComponent();
    VisibleMesh = nullptr;
    CurrentAppearance = nullptr;
}

void UPGCharacterAppearanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearPresentation();
    Super::EndPlay(Reason);
}

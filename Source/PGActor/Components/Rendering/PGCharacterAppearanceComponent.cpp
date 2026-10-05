#include "PGCharacterAppearanceComponent.h"
#include "PGAppearanceAnimInstance.h"
#include "PGToonPresentationComponent.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
    for (const auto& Attachment : Appearance->Attachments)
        if (!Attachment.Mesh.LoadSynchronous() || !Attachment.RelativeTransform.IsValid() ||
            Mesh->GetRefSkeleton().FindBoneIndex(Attachment.AttachBone) == INDEX_NONE) return false;
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
                if (Root->GetAttachParent() == Pair.Component)
                { Actor->AttachToComponent(Source, FAttachmentTransformRules::KeepRelativeTransform, Pair.SourceSocket); break; }
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
    Anim->Appearance = Appearance;
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
    for (const auto& Definition : Appearance->Attachments)
    {
        auto* Part = NewObject<UStaticMeshComponent>(Character);
        Part->SetupAttachment(VisibleMesh, Definition.AttachBone);
        Part->SetRelativeTransform(Definition.RelativeTransform);
        Part->SetStaticMesh(Definition.Mesh.LoadSynchronous());
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetRenderCustomDepth(true);
        Part->SetCustomDepthStencilValue(73);
        Part->SetReceivesDecals(false);
        Part->RegisterComponent();
        Attachments.Add(Part);
    }
    CurrentAppearance = Appearance;
    if (auto* Dash = Character->FindComponentByClass<UPGPlayerDashComponent>()) Dash->PrepareAfterimages();
    for (auto* Actor : Attached)
        if (auto* Root = Actor->GetRootComponent(); Root && Root->GetAttachParent() == Source)
        {
            FName Socket = Root->GetAttachSocketName();
            const auto* PGCharacter = Cast<APGCharacterBase>(Character);
            const auto* Combat = PGCharacter ? PGCharacter->GetCombatComponent() : nullptr;
            const FGameplayTag WeaponTag = Combat ? Combat->GetCarriedWeaponTag(Cast<APGWeaponBase>(Actor)) : FGameplayTag();
            auto* Attachment = ResolveEquipmentAttachment(Socket, WeaponTag);
            Actor->AttachToComponent(Attachment, FAttachmentTransformRules::KeepRelativeTransform, Socket);
        }
    return true;
}

USceneComponent* UPGCharacterAppearanceComponent::ResolveEquipmentAttachment(FName& Socket, const FGameplayTag& WeaponTag)
{
    auto* Character = Cast<ACharacter>(GetOwner());
    auto* Source = Character ? Character->GetMesh() : nullptr;
    if (!Source || !VisibleMesh || !CurrentAppearance || Socket.IsNone()) return Source;
    // Include the carried weapon tag in the cache key. Re-resolve the local transform
    // on each equip so a reused socket cannot retain another weapon's calibration.
    auto* Existing = EquipmentAnchors.FindByPredicate([&](const FPGAppearanceEquipmentAnchor& Entry)
        { return Entry.SourceSocket == Socket && Entry.WeaponTag == WeaponTag; });
    const auto MakeAnchor = [&](FName TargetSocket, const FTransform& Local) -> USceneComponent*
    {
        auto* Anchor = Existing ? Existing->Component.Get() : nullptr;
        if (!IsValid(Anchor))
        {
            Anchor = NewObject<USceneComponent>(Character);
            Anchor->SetupAttachment(VisibleMesh, TargetSocket);
            Anchor->RegisterComponent();
            if (Existing) Existing->Component = Anchor;
            else
            {
                auto& Entry = EquipmentAnchors.AddDefaulted_GetRef();
                Entry.SourceSocket = Socket; Entry.WeaponTag = WeaponTag; Entry.Component = Anchor;
            }
        }
        else Anchor->AttachToComponent(VisibleMesh, FAttachmentTransformRules::KeepRelativeTransform, TargetSocket);
        Anchor->SetRelativeTransform(Local);
        Socket = NAME_None;
        return Anchor;
    };
    const FPGAppearanceGripProfile* Grip = nullptr;
    int32 Matches = 0;
    for (const auto& Profile : CurrentAppearance->GripProfiles)
        if (WeaponTag.IsValid() && Profile.WeaponTag == WeaponTag && Profile.SourceSocket == Socket)
        { Grip = &Profile; ++Matches; }
    // Duplicate/invalid optional data disables that correction only. Explicit target
    // sockets replace the automatic anchor transform rather than compounding it.
    if (Matches == 1 && Grip->GripOffset.IsValid() && !Grip->TargetSocket.IsNone() && VisibleMesh->DoesSocketExist(Grip->TargetSocket))
        return MakeAnchor(Grip->TargetSocket, Grip->GripOffset);
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
    return MakeAnchor(*TargetBone, SocketRef.GetRelativeTransform(TargetBoneRef));
}

int32 UPGCharacterAppearanceComponent::GetEquippedGripIndex() const
{
    const auto* Character = Cast<APGCharacterBase>(GetOwner());
    const auto* Combat = Character ? Character->GetCombatComponent() : nullptr;
    const auto* Weapon = Combat ? Combat->GetCharacterCurrentEquippedWeapon() : nullptr;
    if (!Weapon || !CurrentAppearance || !VisibleMesh) return INDEX_NONE;
    const auto* Root = Weapon->GetRootComponent();
    const auto* Anchor = EquipmentAnchors.FindByPredicate([Root](const FPGAppearanceEquipmentAnchor& Entry)
        { return Root && Root->GetAttachParent() == Entry.Component; });
    if (!Anchor || Anchor->Component->GetAttachParent() != VisibleMesh) return INDEX_NONE;
    int32 Index = INDEX_NONE;
    for (int32 I = 0; I < CurrentAppearance->GripProfiles.Num(); ++I)
    {
        const auto& Grip = CurrentAppearance->GripProfiles[I];
        if (Grip.WeaponTag != Anchor->WeaponTag || Grip.SourceSocket != Anchor->SourceSocket) continue;
        if (Index != INDEX_NONE) return INDEX_NONE;
        Index = I;
    }
    if (Index == INDEX_NONE) return INDEX_NONE;
    const auto& Grip = CurrentAppearance->GripProfiles[Index];
    return Grip.GripOffset.IsValid() && !Grip.TargetSocket.IsNone() && VisibleMesh->DoesSocketExist(Grip.TargetSocket) &&
        Anchor->Component->GetAttachSocketName() == Grip.TargetSocket ? Index : INDEX_NONE;
}

void UPGCharacterAppearanceComponent::ClearPresentation()
{
    for (const auto& Pair : EquipmentAnchors) if (Pair.Component) Pair.Component->DestroyComponent();
    EquipmentAnchors.Reset();
    for (UPGToonPresentationComponent* Toon : Presentations) if (Toon) { Toon->Initialize(nullptr); Toon->DestroyComponent(); }
    Presentations.Reset();
    for (USkeletalMeshComponent* Part : Parts) if (Part) Part->DestroyComponent();
    Parts.Reset();
    for (UStaticMeshComponent* Part : Attachments) if (Part) Part->DestroyComponent();
    Attachments.Reset();
    if (VisibleMesh) VisibleMesh->DestroyComponent();
    VisibleMesh = nullptr;
    CurrentAppearance = nullptr;
}

void UPGCharacterAppearanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearPresentation();
    Super::EndPlay(Reason);
}

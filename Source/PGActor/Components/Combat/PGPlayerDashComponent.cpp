#include "PGPlayerDashComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"

UPGPlayerDashComponent::UPGPlayerDashComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    AfterimageMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/PlayerCombatFX/M_PGDashAfterimage.M_PGDashAfterimage")));
}

void UPGPlayerDashComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() != NM_DedicatedServer) LoadedMaterial = AfterimageMaterial.LoadSynchronous();
}

void UPGPlayerDashComponent::Start()
{
    if (bDashing) return;
    bDashing = true;
    if (auto* Player = Cast<APGCharacterPlayer>(GetOwner()))
    {
        auto* Capsule = Player->GetCapsuleComponent();
        SavedEnemyResponse = Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1);
        if (SavedEnemyResponse == ECR_Block)
        {
            DashCollisionCapsule = Capsule;
            // Enemy bodies cannot stop either character's movement during a dash.
            // Keep overlaps for combat; world collision and damage rules are unchanged.
            Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
        }
    }
    SinceSnapshot = 0.f;
    LastSnapshotLocation = GetOwner()->GetActorLocation();
    SetComponentTickEnabled(true);
}

void UPGPlayerDashComponent::PrepareAfterimages()
{
    if (bDashing || GetNetMode() == NM_DedicatedServer) return;
    if (!LoadedMaterial) LoadedMaterial = AfterimageMaterial.LoadSynchronous();
    // Build the bounded pool/skin material pipelines while loading or changing appearance,
    // before a short first dash can finish while its render resources are still compiling.
    for (int32 Index = 0; Index < 8; ++Index) Capture();
    for (auto& Ghost : Ghosts)
        for (const auto& Mesh : Ghost.Meshes) Mesh->PrecachePSOs();
    Stop(true);
}

void UPGPlayerDashComponent::Stop(bool bClear)
{
    bDashing = false;
    if (auto* Capsule = DashCollisionCapsule.Get())
    {
        DashCollisionCapsule.Reset();
        Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, SavedEnemyResponse);
    }
    if (bClear)
        for (auto& Ghost : Ghosts)
        {
            Ghost.Age = 100.f;
            for (const auto& Mesh : Ghost.Meshes) Mesh->SetVisibility(false);
        }
    if (GetVisibleGhostCount() == 0) SetComponentTickEnabled(false);
}

int32 UPGPlayerDashComponent::GetVisibleGhostCount() const
{
    int32 Count = 0;
    for (const auto& Ghost : Ghosts) if (Ghost.Age < FadeSeconds) ++Count;
    return Count;
}

void UPGPlayerDashComponent::Capture()
{
    if (!LoadedMaterial || GetNetMode() == NM_DedicatedServer) return;
    TArray<USkeletalMeshComponent*> Sources;
    GetOwner()->GetComponents(Sources);
    Sources.RemoveAll([](const auto* Mesh) { return !Mesh->GetSkeletalMeshAsset() || !Mesh->IsVisible() || Mesh->bHiddenInGame; });
    if (Sources.IsEmpty()) return;
    if (Ghosts.Num() < 8) Ghosts.SetNum(8);
    auto& Ghost = Ghosts[NextGhost];
    NextGhost = (NextGhost + 1) % Ghosts.Num();
    if (!Ghost.Material) Ghost.Material = UMaterialInstanceDynamic::Create(LoadedMaterial, this);
    Ghost.Material->SetVectorParameterValue(TEXT("Tint"), Tint);
    Ghost.Material->SetScalarParameterValue(TEXT("Opacity"), .48f);
    Ghost.Age = 0.f;
    for (int32 Index = 0; Index < Sources.Num(); ++Index)
    {
        if (!Ghost.Meshes.IsValidIndex(Index))
        {
            auto* Mesh = NewObject<UPoseableMeshComponent>(GetOwner());
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetGenerateOverlapEvents(false);
            Mesh->SetCastShadow(false);
            Mesh->SetCanEverAffectNavigation(false);
            Mesh->RegisterComponent();
            Mesh->SetComponentTickEnabled(false); // Frozen pose is refreshed explicitly on capture.
            Ghost.Meshes.Add(Mesh);
        }
        auto* Mesh = Ghost.Meshes[Index].Get();
        auto* Source = Sources[Index];
        if (Mesh->GetSkinnedAsset() != Source->GetSkeletalMeshAsset()) Mesh->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
        Mesh->SetWorldTransform(Source->GetComponentTransform());
        // Modular followers have no local pose; copy their leader's evaluated pose.
        auto* Leader = Cast<USkeletalMeshComponent>(Source->LeaderPoseComponent.Get());
        Mesh->CopyPoseFromSkeletalComponent(Leader ? Leader : Source);
        Mesh->RefreshBoneTransforms();
        for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot) Mesh->SetMaterial(Slot, Ghost.Material);
        Mesh->SetVisibility(true);
    }
    for (int32 Index = Sources.Num(); Index < Ghost.Meshes.Num(); ++Index) Ghost.Meshes[Index]->SetVisibility(false);
    LastSnapshotLocation = GetOwner()->GetActorLocation();
}

void UPGPlayerDashComponent::TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Dt, Type, Function);
    const auto* Player = Cast<APGCharacterPlayer>(GetOwner());
    if (!Player || Player->GetPGAbilitySystemComponent()->GetHealth() <= 0.f) { Stop(true); return; }
    for (auto& Ghost : Ghosts)
    {
        if (Ghost.Age >= FadeSeconds) continue;
        Ghost.Age += Dt;
        const float Alpha = FMath::Clamp(1.f - Ghost.Age / FMath::Max(.05f, FadeSeconds), 0.f, 1.f);
        Ghost.Material->SetScalarParameterValue(TEXT("Opacity"), .48f * Alpha * Alpha);
        if (Alpha <= 0.f) for (const auto& Mesh : Ghost.Meshes) Mesh->SetVisibility(false);
    }
    SinceSnapshot += Dt;
    if (bDashing && SinceSnapshot >= FMath::Max(.02f, SnapshotInterval) &&
        FVector::DistSquared(GetOwner()->GetActorLocation(), LastSnapshotLocation) >= FMath::Square(20.f))
    {
        Capture();
        SinceSnapshot = 0.f; // Never emit a pile of identical poses after a stalled frame.
    }
    if (!bDashing && GetVisibleGhostCount() == 0) SetComponentTickEnabled(false);
}

void UPGPlayerDashComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Stop(true);
    for (auto& Ghost : Ghosts) for (const auto& Mesh : Ghost.Meshes) Mesh->DestroyComponent();
    Ghosts.Empty();
    Super::EndPlay(Reason);
}

#include "PGEnemyPresentationComponent.h"
#include "PGData/DataAsset/Combat/PGEnemyPresentationData.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"

UPGEnemyPresentationComponent::UPGEnemyPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPGEnemyPresentationComponent::Initialize(UPGEnemyPresentationData* InData)
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetOwner());
    if (!InData || Data || !Enemy || !Enemy->GetMesh()) return;
    Data = InData;
    for (int32 Index = 0; Index < Data->Armor.Num(); ++Index)
    {
        const auto& Piece = Data->Armor[Index];
        if (!Enemy->GetMesh()->DoesSocketExist(Piece.Socket))
        {
            UE_LOG(LogTemp, Warning, TEXT("PGEnemyPresentation missing socket %s on %s"), *Piece.Socket.ToString(), *Enemy->GetName());
            continue;
        }
        auto* Mesh = Piece.Mesh.LoadSynchronous();
        if (!Mesh) continue;
        auto* Component = NewObject<UStaticMeshComponent>(Enemy, Piece.Name);
        Component->SetStaticMesh(Mesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->SetReceivesDecals(false);
        Component->SetupAttachment(Enemy->GetMesh(), Piece.Socket);
        Component->SetRelativeTransform(Piece.GuardTransform);
        Enemy->AddInstanceComponent(Component);
        Component->RegisterComponent();
        Armor.Add(Component);
        ArmorIndices.Add(Index);
        for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
            if (auto* Material = Component->CreateDynamicMaterialInstance(Slot)) Materials.Add(Material);
    }
    for (const auto& Path : {Data->WindupSound.ToSoftObjectPath(), Data->AimLockSound.ToSoftObjectPath(),
        Data->RecoverySound.ToSoftObjectPath(), Data->GuardHitSound.ToSoftObjectPath(), Data->GuardHitVFX.ToSoftObjectPath()})
        if (auto* Asset = Path.TryLoad()) PreparedAssets.Add(Asset);
    SetGuarding(Enemy->bGuarding);
}

void UPGEnemyPresentationComponent::SetGuarding(bool bEnabled)
{
    if (!Data || bStopped) return;
    if (bGuarding == bEnabled && FMath::IsNearlyEqual(ExposureAlpha, bEnabled ? 0.f : 1.f) && !bWindup) return;
    bGuarding = bEnabled;
    TargetExposure = bEnabled ? 0.f : 1.f;
    StartUpdating();
}

void UPGEnemyPresentationComponent::BeginWindup(float Duration, float AimTrackingSeconds)
{
    if (!Data || bStopped) return;
    bWindup = true;
    bAimLocked = false;
    WindupStartedAt = GetWorld()->GetTimeSeconds();
    WindupDuration = FMath::Max(.05f, Duration);
    AimLockAfter = FMath::Clamp(AimTrackingSeconds, 0.f, WindupDuration);
    PlaySound(Data->WindupSound.Get());
    StartUpdating();
}

void UPGEnemyPresentationComponent::BeginRecovery()
{
    if (!Data || bStopped) return;
    bWindup = bAimLocked = false;
    SetGuarding(false);
    PlaySound(Data->RecoverySound.Get());
}

void UPGEnemyPresentationComponent::ResetPresentation(bool bDead)
{
    if (!Data || bStopped) return;
    bWindup = bAimLocked = false;
    bStopped = bDead;
    if (bDead)
    {
        GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
        bGuarding = false;
        ExposureAlpha = TargetExposure = 1.f;
        ApplyAppearance(0.f);
    }
    else SetGuarding(bGuarding);
}

void UPGEnemyPresentationComponent::StartUpdating()
{
    if (!GetWorld()->GetTimerManager().IsTimerActive(UpdateTimer))
    {
        LastUpdateAt = GetWorld()->GetTimeSeconds();
        GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdatePresentation, 1.f / 30.f, true);
    }
}

void UPGEnemyPresentationComponent::UpdatePresentation()
{
    const double Now = GetWorld()->GetTimeSeconds();
    const float Delta = FMath::Clamp(float(Now - LastUpdateAt), 0.f, .1f);
    LastUpdateAt = Now;
    ExposureAlpha = FMath::FInterpConstantTo(ExposureAlpha, TargetExposure, Delta, 1.f / FMath::Max(.01f, Data->PoseBlendSeconds));
    if (bWindup && !bAimLocked && Now - WindupStartedAt >= AimLockAfter)
    {
        bAimLocked = true;
        PlaySound(Data->AimLockSound.Get());
    }
    ApplyAppearance(bWindup ? FMath::Clamp(float(Now - WindupStartedAt) / WindupDuration, 0.f, 1.f) : 0.f);
    if (!bWindup && FMath::IsNearlyEqual(ExposureAlpha, TargetExposure)) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
}

void UPGEnemyPresentationComponent::ApplyAppearance(float Charge)
{
    for (int32 Index = 0; Index < Armor.Num(); ++Index)
    {
        const auto& Piece = Data->Armor[ArmorIndices[Index]];
        FTransform Pose;
        Pose.Blend(Piece.GuardTransform, Piece.RecoveryTransform, FMath::SmoothStep(0.f, 1.f, ExposureAlpha));
        Armor[Index]->SetRelativeTransform(Pose);
    }
    const FLinearColor Color = bWindup ? FMath::Lerp(Data->GuardColor, Data->WindupColor, Charge) : Data->GuardColor;
    const float Glow = bGuarding ? Data->GuardEmission * (1.f + Charge * .5f) : Data->ExposedEmission;
    for (const auto& Material : Materials)
    {
        Material->SetVectorParameterValue(TEXT("StateColor"), Color);
        Material->SetScalarParameterValue(TEXT("StateGlow"), Glow);
    }
}

void UPGEnemyPresentationComponent::PlaySound(USoundBase* Sound) const
{
    const auto* Enemy = Cast<APGCharacterEnemy>(GetOwner());
    if (Sound && Enemy && Enemy->GetFeedbackIntensity() > 0.f)
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Enemy->GetActorLocation(), FMath::Clamp(Enemy->GetFeedbackIntensity(), 0.f, 1.f));
}

bool UPGEnemyPresentationComponent::PlayGuardImpact()
{
    if (!Data || bStopped) return false;
    auto* Enemy = Cast<APGCharacterEnemy>(GetOwner());
    if (!Enemy || Enemy->GetFeedbackIntensity() <= 0.f) return true;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextGuardHitAt) return true;
    NextGuardHitAt = Now + FMath::Max(.01f, Data->GuardHitInterval);
    PlaySound(Data->GuardHitSound.Get());
    const FVector Location = Enemy->GetActorLocation() + Enemy->GetActorForwardVector() * 40.f;
    if (auto* VFX = Data->GuardHitVFX.Get())
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, Location, Enemy->GetActorRotation(), FVector(Data->GuardHitVFXScale));
    return true;
}

void UPGEnemyPresentationComponent::SetDissolve(float Amount)
{
    for (const auto& Material : Materials) Material->SetScalarParameterValue(TEXT("DissolveAmount"), Amount);
}

void UPGEnemyPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
    Super::EndPlay(Reason);
}

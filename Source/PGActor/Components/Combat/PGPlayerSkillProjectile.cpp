#include "PGPlayerSkillProjectile.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PGShared/Shared/Combat/PGSkillObservation.h"
#include "PGPlayerSlashFX.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

APGPlayerSkillProjectile::APGPlayerSkillProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blade"));
    SetRootComponent(Visual);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetGenerateOverlapEvents(false);
    Visual->SetCanEverAffectNavigation(false);
    Visual->SetCastShadow(false);
}
void APGPlayerSkillProjectile::Initialize(const UPGPlayerSkillProfile* Profile, const FPGPlayerHitPhase& Phase,
    TSharedPtr<FPGSkillCastContext> Context, const FVector& Direction)
{
    CastContext = MoveTemp(Context); Hit = Phase; Forward = Direction.GetSafeNormal2D();
    Observation = CastContext->Observation.Pin();
    if (Observation) ++Observation->Dependents;
    Speed = Profile->ProjectileSpeed; RemainingRange = Profile->ProjectileRange; RemainingTime = Profile->ProjectileLifetime;
    VisualLifetime = FMath::Max(.001f, FMath::Min(RemainingTime, RemainingRange / Speed));
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It) { Stage = *It; StageId = It->GetCurrentStageId(); break; }
    VisualProfile = DuplicateObject<UPGPlayerSkillProfile>(Profile, this);
    NiagaraSlash = PGPlayerSlashFX::Spawn(this, Profile->bUseAuthoredVFX ? nullptr : Profile->SlashVFX.LoadSynchronous(), Profile,
        Hit.Radius, GetActorLocation(), Forward.Rotation(), Profile->bReverseSlash);
    if (NiagaraSlash)
    {
        NiagaraSlash->AttachToComponent(Visual, FAttachmentTransformRules::KeepWorldTransform);
        NiagaraSlash->AddTickPrerequisiteActor(this);
    }
    else
    {
    Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Visual->SetWorldScale3D(FVector(Hit.Radius/50.f,Hit.Radius/50.f,1));
    if (auto* Material = Profile->SlashMaterial.LoadSynchronous())
    {
        SlashMID = UMaterialInstanceDynamic::Create(Material,this);
        PGPlayerSlashFX::SetMaterialBuild(SlashMID, Profile);
        SlashMID->SetScalarParameterValue(TEXT("Shape"), static_cast<float>(EPGPlayerVFXShape::Blade));
        SlashMID->SetScalarParameterValue(TEXT("HalfAngleCos"),.25f);
        SlashMID->SetVectorParameterValue(TEXT("Tint"),Profile->SlashTint);
        SlashMID->SetScalarParameterValue(TEXT("BladeWidth"),Profile->SlashWidth);
        SlashMID->SetScalarParameterValue(TEXT("Intensity"),Profile->SlashIntensity);
        SlashMID->SetScalarParameterValue(TEXT("Projectile"),1.f);
        SlashMID->SetScalarParameterValue(TEXT("Progress"),0.f);
        Visual->SetMaterial(0,SlashMID);
    }
    }
    if (const auto* Definition = VisualProfile->ExternalVFX.Find(EPGPlayerVFXShape::Blade))
    {
        ExternalReferenceDuration = Definition->ReferenceDuration;
        ExternalBlade = PGPlayerSlashFX::SpawnExternal(this, *Definition, VisualProfile, Hit.Radius,
            GetActorLocation(), Forward.Rotation(), Profile->bReverseSlash);
        if (ExternalBlade)
        {
            ExternalBlade->AttachToComponent(Visual, FAttachmentTransformRules::KeepWorldTransform);
            ExternalBlade->AddTickPrerequisiteActor(this);
        }
    }
    Sweep(0.f); // Include enemies already touching the spawn volume.
}
void APGPlayerSkillProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
    PGPlayerSlashFX::Release(NiagaraSlash); NiagaraSlash = nullptr;
    PGPlayerSlashFX::Release(ExternalBlade); ExternalBlade = nullptr;
    VisualProfile = nullptr;
    if (Observation)
    {
        --Observation->Dependents;
        if (Observation->bEndRequested)
            if (auto* Player=Cast<APGCharacterPlayer>(CastContext->Caster.Get()))
                Player->GetPGAbilitySystemComponent()->EndSkillObservation(Observation,Observation->bCancelled);
        Observation.Reset();
    }
    CastContext.Reset();
    Super::EndPlay(Reason);
}
void APGPlayerSkillProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Player = CastContext ? Cast<APGCharacterPlayer>(CastContext->Caster.Get()) : nullptr;
    if (!Player || Player->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        (Stage.IsValid() && (Stage->GetCurrentStageId()!=StageId || Stage->GetCurrentStageState()==EPGStageState::Failed || Stage->GetCurrentStageState()==EPGStageState::Finished)))
    { Destroy(); return; }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
    const float Step = FMath::Min(DeltaSeconds,RemainingTime);
    Sweep(FMath::Min(RemainingRange,Speed*Step));
    RemainingTime -= Step;
    if (IsActorBeingDestroyed()) return;
    PGPlayerSlashFX::SetExternalProgress(ExternalBlade, ExternalReferenceDuration, FMath::Clamp(
        1.f - FMath::Min(RemainingTime, RemainingRange / Speed) / VisualLifetime, 0.f, 1.f));
    PGPlayerSlashFX::SetProgress(NiagaraSlash, VisualProfile, FMath::Clamp(
        1.f - FMath::Min(RemainingTime, RemainingRange / Speed) / VisualLifetime, 0.f, 1.f));
    if (SlashMID) SlashMID->SetScalarParameterValue(TEXT("Progress"), FMath::Clamp(
        1.f - FMath::Min(RemainingTime, RemainingRange / Speed) / VisualLifetime, 0.f, 1.f));
    if (RemainingTime <= 0 || RemainingRange <= UE_SMALL_NUMBER) Destroy();
}
void APGPlayerSkillProjectile::Sweep(float Distance)
{
    const auto Context = CastContext;
    auto* Player = Context ? Cast<APGCharacterPlayer>(Context->Caster.Get()) : nullptr;
    if (!Player) { Destroy(); return; }
    const FVector From = GetActorLocation();
    FVector To = From + Forward*Distance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGBladeSweep),false,Player);
    Params.AddIgnoredActor(this);
    // Characters must not stop penetration; geometry still blocks the complete blade width.
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
    FHitResult Wall;
    const bool Blocked = GetWorld()->SweepSingleByChannel(Wall,From,To,FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeBox(FVector(Hit.Radius,Hit.Radius,20)),Params);
    if (Blocked) To = FMath::Lerp(From,To,FMath::Max(0.f,Wall.Time-.001f));
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    FCollisionQueryParams TargetsQuery(SCENE_QUERY_STAT(PGBladeTargets),false,Player);
    TArray<FHitResult> Targets;
    GetWorld()->SweepMultiByObjectType(Targets,From,To,FQuat::Identity,Objects,FCollisionShape::MakeSphere(Hit.Radius),TargetsQuery);
    ++Context->SpatialQueries;
    Targets.Sort([](const FHitResult& A,const FHitResult& B){ return A.Time==B.Time ? GetNameSafe(A.GetActor())<GetNameSafe(B.GetActor()) : A.Time<B.Time; });
    for (const auto& Contact : Targets)
    {
        if (IsActorBeingDestroyed() || Player->GetPGAbilitySystemComponent()->GetHealth() <= 0) break;
        auto* Enemy = Cast<APGCharacterEnemy>(Contact.GetActor());
        if (!IsValid(Enemy) || Contact.GetComponent()!=Enemy->GetCapsuleComponent()) continue;
        const float FeetZ = Enemy->GetActorLocation().Z-Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (FMath::Abs(FeetZ-(From.Z-80.f))>Hit.HeightTolerance) continue;
        FHitResult Occlusion;
        if (GetWorld()->LineTraceSingleByChannel(Occlusion,From,Enemy->GetActorLocation(),ECC_Pawn,Params)) continue;
        Player->GetPGAbilitySystemComponent()->ApplyPlayerProfileHit(Enemy,Context,Hit.PhaseId,
            Hit.DamageMultiplier,Hit.bHeavyImpact,Hit.ProcPolicy,Hit.HitStopSeconds);
    }
    SetActorLocation(To);
    RemainingRange -= Distance;
    if (Blocked) Destroy();
}

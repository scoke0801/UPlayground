#include "PGPlayerAttackComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPGSkillShapes(TEXT("pg.Skill.DebugShapes"), 0, TEXT("Draw player profile hit geometry."));
static TAutoConsoleVariable<int32> CVarPGSkillCast(TEXT("pg.Skill.DebugCast"), 0, TEXT("Log committed player casts and phase targets."));

namespace
{
FVector Feet(const ACharacter* Character)
{
    return Character->GetActorLocation() - FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}
FCollisionObjectQueryParams WorldObjects()
{
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    return Objects;
}
}

UPGPlayerAttackComponent::UPGPlayerAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    // Input/cancellation and character movement are evaluated before damage.
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UPGPlayerAttackComponent::PrepareLoadout()
{
    auto* Player = Cast<APGCharacterPlayer>(GetOwner());
    auto* Tables = UPGDataTableManager::Get(this);
    if (!Player || !Player->GetSkillHandler() || !Tables || IsRunning()) return;
    PreparedLoadoutAssets.Reset();
    TSet<int32> IDs;
    for (const auto& Pair : Player->GetSkillHandler()->GetAllSkillData())
    {
        IDs.Add(Pair.Value.SkillId);
        if (const auto* Row = Tables->GetRowData<FPGSkillDataRow>(Pair.Value.SkillId))
            for (int32 Chain : Row->ChainSkillIdList) IDs.Add(Chain);
    }
    for (int32 ID : IDs)
        if (const auto* Row = Tables->GetRowData<FPGSkillDataRow>(ID))
            if (auto* Profile = Row->PlayerProfile.LoadSynchronous())
            {
                PreparedLoadoutAssets.Add(Profile);
                for (const auto& Path : {Profile->SlashMaterial.ToSoftObjectPath(), Profile->SlashVFX.ToSoftObjectPath(), Profile->SwingSound.ToSoftObjectPath()})
                    if (!Path.IsNull()) if (auto* Asset = Path.TryLoad()) PreparedLoadoutAssets.Add(Asset);
            }
}

bool UPGPlayerAttackComponent::CanPrepare(const UPGPlayerSkillProfile* Profile, const UAnimMontage* Montage, FString& Error) const
{
    const auto* Player = Cast<APGCharacterPlayer>(GetOwner());
    if (!Player || !Profile || !Montage || !Player->GetMesh()->GetAnimInstance() ||
        !Profile->Validate(Profile->SkillID, Error)) return false;
    if (Profile->PoseKeys.Last().MontageSeconds > Montage->GetPlayLength() ||
        !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0.f)
    { Error = TEXT("Pose mapping exceeds montage or invalid RateScale"); return false; }
    if (Player->GetCharacterMovement()->IsFalling() || Player->GetActorLocation().ContainsNaN())
    { Error = TEXT("Player has no grounded cast origin"); return false; }
    FHitResult Floor;
    const FVector Origin = Feet(Player);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGSkillPreflight), false, Player);
    if (!GetWorld()->LineTraceSingleByObjectType(Floor, Origin + FVector(0,0,45), Origin - FVector(0,0,50), WorldObjects(), Params) ||
        Floor.ImpactNormal.Z < Player->GetCharacterMovement()->GetWalkableFloorZ())
    { Error = TEXT("No walkable ground at cast origin"); return false; }
    return true;
}

bool UPGPlayerAttackComponent::Start(const UPGPlayerSkillProfile* Profile, UAnimMontage* Montage,
    bool bRefundEligible, FPGPlayerAttackEnded OnEnded)
{
    FString Error;
    if (IsRunning() || !CanPrepare(Profile, Montage, Error)) return false;
    auto* Player = CastChecked<APGCharacterPlayer>(GetOwner());
    auto* ASC = Player->GetPGAbilitySystemComponent();
    // Snapshot the complete profile, not an editor asset that could change mid-cast.
    ActiveProfile = DuplicateObject<UPGPlayerSkillProfile>(Profile, this);
    ActiveMontage = Montage;
    PreparedVFX = Profile->SlashVFX.LoadSynchronous();
    PreparedSound = Profile->SwingSound.LoadSynchronous();
    if (SlashMesh) SlashMesh->SetVisibility(false);
    SlashMID = nullptr;
    if (auto* Material = Profile->SlashMaterial.LoadSynchronous())
    {
        if (!SlashMesh)
        {
            SlashMesh = NewObject<UStaticMeshComponent>(Player);
            SlashMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
            SlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SlashMesh->SetGenerateOverlapEvents(false);
            SlashMesh->SetCanEverAffectNavigation(false);
            SlashMesh->SetCastShadow(false);
            Player->AddInstanceComponent(SlashMesh); SlashMesh->RegisterComponent();
        }
        SlashMID = UMaterialInstanceDynamic::Create(Material, this);
        SlashMesh->SetMaterial(0, SlashMID); SlashMesh->SetVisibility(false);
    }
    CastContext = MakeShared<FPGSkillCastContext>();
    CastContext->SkillID = Profile->SkillID;
    CastContext->Caster = Player;
    CastContext->Attack = ASC->GetCombatStat(EPGStatType::Attack);
    CastContext->bRefundEligible = bRefundEligible;
    CastContext->FrenzyCap = Profile->FrenzyPerCastCap;
    Ended = MoveTemp(OnEnded);
    LogicalTime = 0.f;
    Speed = FMath::Clamp(Profile->AttackSpeed * ASC->GetFrenzyRate(), .75f, 1.75f);
    bAimLocked = false; PresentedPhases.Reset();
    Player->FaceAimDirection(); LockedForward = Player->GetActorForwardVector();
    Player->SetAttackAimTracking(false);
    SavedWalkSpeed = Player->GetCharacterMovement()->MaxWalkSpeed;
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->GetCharacterMovement()->MaxWalkSpeed = 0.f;
    auto* Anim = Player->GetMesh()->GetAnimInstance();
    SavedRootMotionMode = Anim->RootMotionMode;
    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    // The scheduler owns pose sampling as well as hits: RateScale/legacy notifies cannot double-drive it.
    Anim->Montage_SetPlayRate(Montage, 0.f);
    Anim->Montage_SetPosition(Montage, ActiveProfile->GetMontagePosition(0.f));
    Player->GetCombatComponent()->ToggleWeaponCollision(false, EPGToggleDamageType::CurrentEquippedWeapon);
    SetComponentTickEnabled(true);
    if (CVarPGSkillCast.GetValueOnGameThread())
        UE_LOG(LogTemp, Log, TEXT("PGSkill Cast=%s Skill=%d Begin speed=%.3f attack=%.2f"),
            *CastContext->CastId.ToString(), Profile->SkillID, Speed, CastContext->Attack);
    return true;
}

void UPGPlayerAttackComponent::Stop(bool bNotify, bool bCancelled)
{
    if (!IsRunning()) return;
    if (CVarPGSkillCast.GetValueOnGameThread())
        UE_LOG(LogTemp, Log, TEXT("PGSkill Cast=%s End cancelled=%d time=%.3f frenzy=%d shock=%d refund=%d"),
            *CastContext->CastId.ToString(), bCancelled, LogicalTime, CastContext->FrenzyGranted,
            CastContext->bShockUsed, CastContext->bRefundUsed);
    auto Callback = MoveTemp(Ended);
    CastContext.Reset();
    if (SlashMesh) SlashMesh->SetVisibility(false);
    SetComponentTickEnabled(false);
    if (auto* Player = Cast<APGCharacterPlayer>(GetOwner()))
    {
        Player->GetCharacterMovement()->MaxWalkSpeed = SavedWalkSpeed;
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->SetAttackAimTracking(false);
        Player->ResetAttackHitStop();
        if (auto* Anim = Player->GetMesh()->GetAnimInstance())
        {
            Anim->SetRootMotionMode(static_cast<ERootMotionMode::Type>(SavedRootMotionMode));
            Anim->Montage_SetPlayRate(ActiveMontage, 1.f);
        }
        Player->GetCombatComponent()->ToggleWeaponCollision(false, EPGToggleDamageType::CurrentEquippedWeapon);
    }
    ActiveProfile = nullptr; ActiveMontage = nullptr; PreparedVFX = nullptr; PreparedSound = nullptr;
    if (bNotify) Callback.ExecuteIfBound(bCancelled);
}

void UPGPlayerAttackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Stop(); Super::EndPlay(Reason);
}

bool UPGPlayerAttackComponent::CanCancel(bool bDodge) const
{
    return IsRunning() && LogicalTime >= (bDodge ? ActiveProfile->DodgeCancel : ActiveProfile->AttackCancel);
}
float UPGPlayerAttackComponent::GetExpectedSeconds() const { return IsRunning() ? ActiveProfile->Duration / Speed : 0.f; }

void UPGPlayerAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    auto* Player = Cast<APGCharacterPlayer>(GetOwner());
    if (!IsRunning() || !Player) return;
    // Hold the slash with the sampled pose during hit-stop, including the first impact.
    if (SlashMesh && LogicalTime >= SlashUntil) SlashMesh->SetVisibility(false);
    if (Player->GetPGAbilitySystemComponent()->GetHealth() <= 0.f || !Player->IsGameplayInputAllowed())
    { Stop(true); return; }
    if (Player->GetMesh()->GlobalAnimRateScale <= 0.f)
    {
        Player->GetCharacterMovement()->MaxWalkSpeed = 0.f;
        Player->GetCharacterMovement()->StopMovementImmediately();
        return;
    }
    Advance(DeltaTime * Speed);
}

void UPGPlayerAttackComponent::Advance(float Seconds)
{
    if (!IsRunning() || !FMath::IsFinite(Seconds) || Seconds <= 0.f) return;
    const auto Context = CastContext;
    auto* Player = CastChecked<APGCharacterPlayer>(GetOwner());
    const float Until = FMath::Min(ActiveProfile->Duration, LogicalTime + Seconds);
    // Split at phase/movement boundaries and at 120 Hz so a long frame cannot skip a window
    // or evaluate both hits at the final dash position.
    while (CastContext == Context && LogicalTime < Until)
    {
        float Next = FMath::Min(Until, LogicalTime + 1.f / 120.f);
        const auto Boundary = [&](float Value) { if (Value > LogicalTime + SMALL_NUMBER) Next = FMath::Min(Next, Value); };
        Boundary(ActiveProfile->AimLock);
        for (const auto& Hit : ActiveProfile->HitPhases) { Boundary(Hit.Start); Boundary(Hit.End); }
        for (const auto& Move : ActiveProfile->MovementSegments) { Boundary(Move.Start); Boundary(Move.End); }
        if (!bAimLocked)
        {
            Player->FaceAimDirection(); LockedForward = Player->GetActorForwardVector();
            bAimLocked = Next >= ActiveProfile->AimLock;
        }
        const float Previous = LogicalTime;
        if (!MoveBetween(Previous, Next)) { Stop(true); return; }
        LogicalTime = Next;
        for (const auto& Hit : ActiveProfile->HitPhases)
        {
            if (Next >= Hit.Start && Next <= Hit.End)
            {
                if (!PresentedPhases.Contains(Hit.PhaseId)) { PresentedPhases.Add(Hit.PhaseId); PresentHit(Hit); }
                QueryHit(Hit);
                if (CastContext != Context) return; // death/cancel/reentrant callbacks
            }
        }
    }
    if (CastContext != Context) return;
    if (auto* Anim = Player->GetMesh()->GetAnimInstance())
        Anim->Montage_SetPosition(ActiveMontage, ActiveProfile->GetMontagePosition(LogicalTime));
    if (LogicalTime >= ActiveProfile->Duration) Stop(true, false);
}

bool UPGPlayerAttackComponent::MoveBetween(float From, float To)
{
    auto* Player = CastChecked<APGCharacterPlayer>(GetOwner());
    auto* Movement = Player->GetCharacterMovement();
    Movement->MaxWalkSpeed = 0.f;
    for (const auto& Move : ActiveProfile->MovementSegments)
    {
        if (To < Move.Start || From >= Move.End) continue;
        if (Move.Mode == EPGPlayerMoveMode::Walk)
        {
            Movement->MaxWalkSpeed = SavedWalkSpeed * Move.WalkSpeedRatio;
            continue;
        }
        const float Fraction = (FMath::Clamp(To, Move.Start, Move.End) - FMath::Clamp(From, Move.Start, Move.End)) / (Move.End - Move.Start);
        const FVector Delta = LockedForward * Move.Distance * Fraction;
        if (Delta.IsNearlyZero()) continue;
        // Ground support before moving; a dash never bridges a gap or teleports up a floor.
        const FVector DesiredFeet = Feet(Player) + Delta;
        FHitResult Floor;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PGSkillFloor), false, Player);
        if (!GetWorld()->LineTraceSingleByObjectType(Floor, DesiredFeet + FVector(0,0,Movement->MaxStepHeight),
            DesiredFeet - FVector(0,0,Movement->MaxStepHeight + 5.f), WorldObjects(), Params) ||
            Floor.ImpactNormal.Z < Movement->GetWalkableFloorZ())
        {
            if (CVarPGSkillCast.GetValueOnGameThread()) UE_LOG(LogTemp, Log, TEXT("PGSkill Movement no floor at %s hit=%s normal=%s"), *DesiredFeet.ToString(), *GetNameSafe(Floor.GetActor()), *Floor.ImpactNormal.ToString());
            return !Move.bEndCastOnBlock;
        }
        // EnemyCharacter overlaps the project's Player channel. An explicit capsule query
        // prevents that legacy response from allowing a dash through an elite/boss body.
        // P0 conservatively stops at every living enemy; no temporary collision overrides.
        FCollisionObjectQueryParams EnemyObjects;
        EnemyObjects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
        EnemyObjects.AddObjectTypesToQuery(ECC_Pawn);
        TArray<FHitResult> Bodies;
        const auto* Capsule = Player->GetCapsuleComponent();
        GetWorld()->SweepMultiByObjectType(Bodies, Player->GetActorLocation(), Player->GetActorLocation() + Delta,
            FQuat::Identity, EnemyObjects, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
        float SafeFraction = 1.f;
        bool bEnemyBlocked = false;
        for (const auto& Body : Bodies)
            if (auto* Enemy = Cast<APGCharacterEnemy>(Body.GetActor()); Enemy &&
                Body.GetComponent() == Enemy->GetCapsuleComponent() && Enemy->GetPGAbilitySystemComponent()->GetHealth() > 0.f)
            { SafeFraction = FMath::Min(SafeFraction, FMath::Max(0.f, Body.Time - .001f)); bEnemyBlocked = true; }
        FHitResult Block;
        Player->SetActorLocation(Player->GetActorLocation() + Delta * SafeFraction, true, &Block);
        if ((Block.bBlockingHit || bEnemyBlocked) && Move.bEndCastOnBlock)
        {
            if (CVarPGSkillCast.GetValueOnGameThread()) UE_LOG(LogTemp, Log, TEXT("PGSkill Movement blocked at %s by %s penetrating=%d"), *Player->GetActorLocation().ToString(), *GetNameSafe(Block.GetActor()), Block.bStartPenetrating);
            return false;
        }
    }
    return true;
}

bool UPGPlayerAttackComponent::ContainsTarget(const FPGPlayerHitPhase& Hit, const FVector& Origin,
    const FVector& Forward, const FVector& TargetFeet, float CapsuleRadius)
{
    const FVector Offset = TargetFeet - Origin;
    if (Offset.ContainsNaN() || FMath::Abs(Offset.Z) > Hit.HeightTolerance) return false;
    const float Distance = Offset.Size2D();
    if (Distance > Hit.Radius + CapsuleRadius) return false;
    if (Hit.Shape == EPGPlayerHitShape::Disc || Distance <= CapsuleRadius) return true;
    const FVector Direction = Offset.GetSafeNormal2D();
    const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Forward.GetSafeNormal2D()), -1.f, 1.f));
    const float HalfAngle = FMath::DegreesToRadians(Hit.FullAngleDegrees * .5f);
    if (Angle <= HalfAngle) return true;
    // Distance to the finite fan edge, not an angle expansion at the outer corner.
    const float EdgeAngle = Angle - HalfAngle;
    const float Along = FMath::Clamp(Distance * FMath::Cos(EdgeAngle), 0.f, Hit.Radius);
    return FMath::Square(Distance) + FMath::Square(Along) - 2.f * Distance * Along * FMath::Cos(EdgeAngle) <= FMath::Square(CapsuleRadius);
}

void UPGPlayerAttackComponent::QueryHit(const FPGPlayerHitPhase& Hit)
{
    const auto Context = CastContext;
    ++Context->SpatialQueries;
    auto* Player = CastChecked<APGCharacterPlayer>(GetOwner());
    const FVector Origin = Feet(Player);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGPlayerSkillHit), false, Player);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps, Origin + FVector(0,0,Hit.HeightTolerance), FQuat::Identity,
        Objects, FCollisionShape::MakeBox(FVector(Hit.Radius, Hit.Radius, Hit.HeightTolerance)), Params);
    TArray<APGCharacterEnemy*> Targets;
    for (const auto& Overlap : Overlaps)
        if (auto* Enemy = Cast<APGCharacterEnemy>(Overlap.GetActor())) Targets.AddUnique(Enemy);
    Targets.Sort([Origin](const APGCharacterEnemy& A, const APGCharacterEnemy& B)
    {
        const double DA = FVector::DistSquared2D(A.GetActorLocation(), Origin), DB = FVector::DistSquared2D(B.GetActorLocation(), Origin);
        return DA == DB ? A.GetPathName() < B.GetPathName() : DA < DB;
    });
    for (auto* Enemy : Targets)
    {
        if (CVarPGSkillCast.GetValueOnGameThread() > 1)
            UE_LOG(LogTemp, Log, TEXT("PGSkill Candidate=%s origin=%s feet=%s forward=%s radius=%.1f health=%.1f inside=%d"),
                *GetNameSafe(Enemy), *Origin.ToString(), *Feet(Enemy).ToString(), *LockedForward.ToString(),
                Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius(), Enemy->GetPGAbilitySystemComponent()->GetHealth(),
                ContainsTarget(Hit,Origin,LockedForward,Feet(Enemy),Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius()));
        if (CastContext != Context || Player->GetPGAbilitySystemComponent()->GetHealth() <= 0.f) return;
        if (!IsValid(Enemy) || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0.f ||
            Context->HitTargets.FindOrAdd(Hit.PhaseId).Contains(Enemy) ||
            !ContainsTarget(Hit, Origin, LockedForward, Feet(Enemy), Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius())) continue;
        FHitResult Wall;
        FCollisionQueryParams OcclusionParams = Params;
        for (const auto* Candidate : Targets) OcclusionParams.AddIgnoredActor(Candidate);
        // Pawn-blocking geometry includes invisible walls, but excludes trigger/loot widgets.
        if (Hit.bWallOcclusion && GetWorld()->LineTraceSingleByChannel(Wall, Origin + FVector(0,0,50),
            Feet(Enemy) + FVector(0,0,50), ECC_Pawn, OcclusionParams))
        {
            if (CVarPGSkillCast.GetValueOnGameThread() > 1) UE_LOG(LogTemp, Log, TEXT("PGSkill Occluded by=%s"), *GetNameSafe(Wall.GetActor()));
            continue;
        }
        Player->GetPGAbilitySystemComponent()->ApplyPlayerProfileHit(Enemy, Context, Hit.PhaseId,
            Hit.DamageMultiplier, Hit.bHeavyImpact, Hit.ProcPolicy, Hit.HitStopSeconds);
        if (CVarPGSkillCast.GetValueOnGameThread())
            UE_LOG(LogTemp, Log, TEXT("PGSkill Cast=%s Skill=%d Phase=%d Target=%s time=%.3f"),
                *Context->CastId.ToString(), Context->SkillID, Hit.PhaseId, *GetNameSafe(Enemy), LogicalTime);
    }
}

void UPGPlayerAttackComponent::PresentHit(const FPGPlayerHitPhase& Hit)
{
    const auto* Player = CastChecked<APGCharacterPlayer>(GetOwner());
    const FVector Center = Feet(Player) + FVector(0,0,10);
    if (SlashMesh && SlashMID)
    {
        SlashMID->SetScalarParameterValue(TEXT("HalfAngleCos"), Hit.Shape == EPGPlayerHitShape::Disc ? -1.f : FMath::Cos(FMath::DegreesToRadians(Hit.FullAngleDegrees*.5f)));
        SlashMID->SetVectorParameterValue(TEXT("Tint"), Hit.PhaseId % 2 ? FLinearColor(.55f,.4f,1.f) : FLinearColor(.25f,1.f,.75f));
        SlashMesh->SetWorldLocationAndRotation(Center + FVector(0,0,35), LockedForward.Rotation());
        SlashMesh->SetWorldScale3D(FVector(Hit.Radius / 50.f, Hit.Radius / 50.f, 1.f));
        SlashMesh->SetVisibility(true); SlashUntil = LogicalTime + .1f;
    }
    if (PreparedVFX)
        if (auto* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PreparedVFX, Center,
            LockedForward.Rotation(), FVector::OneVector, true, false, ENCPoolMethod::AutoRelease))
        {
            FX->SetVariableFloat(TEXT("User.Radius"), Hit.Radius);
            FX->SetVariableFloat(TEXT("User.FullAngle"), Hit.Shape == EPGPlayerHitShape::Disc ? 360.f : Hit.FullAngleDegrees);
            FX->Activate();
        }
    if (PreparedSound) UGameplayStatics::PlaySoundAtLocation(this, PreparedSound, Center);
    if (CVarPGSkillShapes.GetValueOnGameThread())
    {
        const float Angle = Hit.Shape == EPGPlayerHitShape::Disc ? 360.f : Hit.FullAngleDegrees;
        FVector Previous = Center + LockedForward.RotateAngleAxis(-Angle*.5f, FVector::UpVector)*Hit.Radius;
        DrawDebugLine(GetWorld(), Center, Previous, FColor::Cyan, false, .15f);
        for (int32 Index=1; Index<=32; ++Index)
        {
            const FVector Next = Center + LockedForward.RotateAngleAxis(-Angle*.5f + Angle*Index/32.f, FVector::UpVector)*Hit.Radius;
            DrawDebugLine(GetWorld(), Previous, Next, FColor::Cyan, false, .15f); Previous=Next;
        }
        DrawDebugLine(GetWorld(), Center, Previous, FColor::Cyan, false, .15f);
    }
}

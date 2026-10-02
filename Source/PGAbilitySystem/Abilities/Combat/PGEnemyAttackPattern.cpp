#include "PGEnemyAbilityAttack.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Projectile/PGPatternProjectile.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPGPatternDebug(TEXT("pg.Combat.EliteDebug"), 0, TEXT("Draw the current shared pattern bounds."));

void UPGEnemyAbilityAttack::BeginElitePattern(const FPGSkillDataRow& Row)
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    if (!Enemy || !Row.IsPatternValid()) { EndAbilitySelf(); return; }
    EliteData = Row;
    bElitePattern = Enemy->bPatternActive = Enemy->bPerformingHeavyAttack = true;
    Enemy->ActivePatternID = Row.SkillID;
    Enemy->bPatternRecovering = Enemy->bPatternStriking = false;
    Enemy->ClearPatternHitboxes();
    PatternTarget = UGameplayStatics::GetPlayerPawn(this, 0);
    PatternOrigin = Enemy->GetActorLocation();
    PatternOrigin.Z -= Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    PatternForward = Enemy->GetActorForwardVector();
    PatternStartedAt = LastUpdateAt = GetWorld()->GetTimeSeconds();
    Travelled = 0; HazardIndex = 0; bTravelling = bStriking = false;
    if (auto* Montage = Row.ElitePresentationMontage.LoadSynchronous())
        if (auto* Anim = Enemy->GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Play(Montage);
            Anim->Montage_SetPosition(Montage, Montage->GetPlayLength() * FMath::Clamp(Row.WindupMontageFraction, 0.f, 1.f));
            Anim->Montage_Pause(Montage);
        }
    auto* Movement = Enemy->GetCharacterMovement();
    SavedMovementMode = Movement->MovementMode;
    Movement->StopMovementImmediately(); Movement->DisableMovement();
    if (auto* AI = Cast<AAIController>(Enemy->GetController())) AI->StopMovement();
    UpdateAim();
    ShowTelegraph(StrikeCenter, Row.Pattern == EPGAttackPattern::ChargeSlam || Row.Pattern == EPGAttackPattern::AimedProjectile);
    GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdatePattern, .02f, true);
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::StrikeElitePattern, FMath::Max(.05f, Row.TelegraphDuration), false);
    UE_LOG(LogTemp, Log, TEXT("PGPattern Windup skill=%d pattern=%d"), Row.SkillID, int32(Row.Pattern));
    Enemy->PublishBossPresentation();
}

void UPGEnemyAbilityAttack::UpdateAim()
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    // Legacy slams keep the original facing and offset; new roles aim only during windup.
    if (EliteData.Pattern != EPGAttackPattern::LegacySlam && PatternTarget.IsValid())
    {
        const FVector Direction = (PatternTarget->GetActorLocation() - PatternOrigin).GetSafeNormal2D();
        if (!Direction.IsNearlyZero()) PatternForward = Direction;
        Enemy->SetActorRotation(PatternForward.Rotation());
    }
    switch (EliteData.Pattern)
    {
    case EPGAttackPattern::LegacySlam: StrikeCenter = PatternOrigin + PatternForward * EliteData.TelegraphRadius; break;
    case EPGAttackPattern::HazardSequence:
        HazardOrigin = PatternTarget.IsValid() ? PatternTarget->GetActorLocation() : PatternOrigin + PatternForward * 400;
        HazardOrigin.Z = PatternOrigin.Z; StrikeCenter = HazardOrigin; break;
    default: StrikeCenter = PatternOrigin; break;
    }
    FHitResult Ground;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGPatternGround), false, Enemy);
    if (GetWorld()->LineTraceSingleByObjectType(Ground, StrikeCenter + FVector(0,0,100), StrikeCenter - FVector(0,0,200),
        FCollisionObjectQueryParams(ECC_WorldStatic), Params)) StrikeCenter.Z = Ground.ImpactPoint.Z;
    if (EliteData.Pattern == EPGAttackPattern::HazardSequence) HazardOrigin = StrikeCenter;
    if (Enemy->bGuarding) Enemy->SetGuarding(true);
}

void UPGEnemyAbilityAttack::ShowTelegraph(const FVector& Center, bool bLine, bool bRecovery)
{
    if (Telegraph) { Telegraph->DestroyComponent(); Telegraph = nullptr; }
    auto* Material = EliteData.TelegraphMaterial.LoadSynchronous();
    if (!Material) return;
    const float Radius = bRecovery ? 100.f : EliteData.TelegraphRadius;
    const float Extent = bLine ? EliteData.TravelDistance + EliteData.TelegraphRadius : Radius;
    Telegraph = UGameplayStatics::SpawnDecalAtLocation(this, Material, FVector(180, Extent, Extent), Center, FRotator(-90,0,0), 0);
    if (Telegraph)
        if (auto* MID = Telegraph->CreateDynamicMaterialInstance())
        {
            MID->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center.X, Center.Y, Center.Z));
            MID->SetVectorParameterValue(TEXT("Forward"), FLinearColor(PatternForward.X, PatternForward.Y, 0));
            MID->SetScalarParameterValue(TEXT("Radius"), Radius);
            MID->SetScalarParameterValue(TEXT("Length"), EliteData.TravelDistance);
            MID->SetScalarParameterValue(TEXT("HalfWidth"), EliteData.LineHalfWidth);
            MID->SetScalarParameterValue(TEXT("Shape"), bLine ? 2.f : (!bRecovery && EliteData.Pattern == EPGAttackPattern::Sweep ? 1.f : 0.f));
            MID->SetScalarParameterValue(TEXT("CosAngle"), FMath::Cos(FMath::DegreesToRadians(EliteData.HalfAngleDegrees)));
            MID->SetVectorParameterValue(TEXT("GradeColor"), bRecovery ? FLinearColor(.05f,.8f,1.5f) : FLinearColor(1,.18f,.025f));
        }
}

void UPGEnemyAbilityAttack::UpdatePattern()
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    auto* Target = Cast<APGCharacterBase>(PatternTarget.Get());
    if (!IsActive() || !IsValid(Enemy) || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        (EliteData.Pattern != EPGAttackPattern::LegacySlam && (!IsValid(Target) || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0))) { EndAbilitySelf(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    const float Delta = FMath::Clamp(float(Now - LastUpdateAt), 0.f, .1f); LastUpdateAt = Now;
    if (bTravelling)
    {
        const float Step = FMath::Min(EliteData.TravelDistance - Travelled, EliteData.TravelSpeed * Delta);
        // Swept horizontal travel alone can leave the arena through an unguarded ledge.
        const FVector Next = Enemy->GetActorLocation() + PatternForward * Step;
        const float HalfHeight = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FHitResult Ground;
        FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(PGChargeGround), false, Enemy);
        const bool bSafeFloor = GetWorld()->LineTraceSingleByObjectType(Ground, Next,
            Next - FVector(0,0,HalfHeight + Enemy->GetCharacterMovement()->MaxStepHeight),
            FCollisionObjectQueryParams(ECC_WorldStatic), GroundParams) &&
            Ground.ImpactNormal.Z >= Enemy->GetCharacterMovement()->GetWalkableFloorZ() &&
            FMath::Abs(Ground.ImpactPoint.Z + HalfHeight - Next.Z) <= Enemy->GetCharacterMovement()->MaxStepHeight;
        FHitResult Hit;
        const FVector Before = Enemy->GetActorLocation();
        if (bSafeFloor) Enemy->AddActorWorldOffset(FVector(Next.X, Next.Y, Ground.ImpactPoint.Z + HalfHeight + 2.f) - Before, true, &Hit);
        Travelled += FVector::Dist2D(Before, Enemy->GetActorLocation());
        if (!bSafeFloor || Travelled >= EliteData.TravelDistance - KINDA_SMALL_NUMBER || Hit.bBlockingHit)
        {
            bTravelling = false;
            StrikeCenter = Enemy->GetActorLocation(); StrikeCenter.Z -= HalfHeight;
            // Landing circle gets its own visible warning after collision-shortened travel.
            ShowTelegraph(StrikeCenter, false);
            GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::StrikeElitePattern, EliteData.LandingTelegraphSeconds, false);
        }
    }
    else if (!bStriking && Now - PatternStartedAt < FMath::Min(EliteData.AimTrackingSeconds, EliteData.TelegraphDuration))
    {
        UpdateAim();
        if (Telegraph)
        {
            Telegraph->SetWorldLocation(StrikeCenter);
            if (auto* MID = Cast<UMaterialInstanceDynamic>(Telegraph->GetDecalMaterial()))
            {
                MID->SetVectorParameterValue(TEXT("Center"), FLinearColor(StrikeCenter.X,StrikeCenter.Y,StrikeCenter.Z));
                MID->SetVectorParameterValue(TEXT("Forward"), FLinearColor(PatternForward.X,PatternForward.Y,0));
            }
        }
    }
    if (CVarPGPatternDebug.GetValueOnGameThread() && !Enemy->bPatternRecovering)
    {
        if (!bStriking && (EliteData.Pattern == EPGAttackPattern::ChargeSlam || EliteData.Pattern == EPGAttackPattern::AimedProjectile))
            DrawDebugBox(GetWorld(), PatternOrigin + PatternForward * EliteData.TravelDistance * .5f,
                FVector(EliteData.TravelDistance * .5f, EliteData.LineHalfWidth, 3), PatternForward.ToOrientationQuat(), FColor::Orange, false, .025f);
        else if (EliteData.Pattern == EPGAttackPattern::Sweep)
            DrawDebugCone(GetWorld(), StrikeCenter + FVector(0,0,3), PatternForward, EliteData.TelegraphRadius,
                FMath::DegreesToRadians(EliteData.HalfAngleDegrees), .001f, 32, FColor::Orange, false, .025f);
        else DrawDebugCircle(GetWorld(), StrikeCenter + FVector(0,0,3), EliteData.TelegraphRadius, 48,
            FColor::Orange, false, .025f, 0, 2, FVector::ForwardVector, FVector::RightVector, false);
    }
}

void UPGEnemyAbilityAttack::StrikeElitePattern()
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    if (!IsActive() || !IsValid(Enemy) || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0) { EndAbilitySelf(); return; }
    if (EliteData.Pattern != EPGAttackPattern::LegacySlam)
    {
        auto* Target = Cast<APGCharacterBase>(PatternTarget.Get());
        if (!IsValid(Target) || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0) { EndAbilitySelf(); return; }
    }
    if (Telegraph) { Telegraph->DestroyComponent(); Telegraph = nullptr; }
    if (!bStriking)
    {
        bStriking = Enemy->bPatternStriking = true;
        Enemy->PublishBossPresentation();
        if (auto* Anim = Enemy->GetMesh()->GetAnimInstance())
            if (auto* Montage = EliteData.ElitePresentationMontage.Get()) Anim->Montage_Resume(Montage);
        if (auto* Sound = EliteData.AttackSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(this, Sound, Enemy->GetActorLocation());
        if (EliteData.Pattern == EPGAttackPattern::ChargeSlam) { bTravelling = true; return; }
        if (EliteData.Pattern == EPGAttackPattern::AimedProjectile)
        {
            auto* Class = EliteData.ProjectileClass.LoadSynchronous();
            FActorSpawnParameters Params; Params.Owner = Enemy; Params.Instigator = Enemy;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector Start = Enemy->GetActorLocation() + PatternForward * 65.f;
            if (auto* Bolt = GetWorld()->SpawnActor<APGPatternProjectile>(Class ? Class : APGPatternProjectile::StaticClass(), Start, PatternForward.Rotation(), Params))
            {
                Bolt->SetMaxTravelDistance(FMath::Max(1.f, EliteData.TravelDistance - 65.f));
                Bolt->SetCollisionHalfWidth(EliteData.LineHalfWidth);
                ActiveProjectile = Bolt;
                Bolt->Fire(Enemy, Start, PatternForward, EliteData.TravelSpeed);
            }
            BeginRecovery(); return;
        }
    }
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel3);
    GetWorld()->OverlapMultiByObjectType(Hits, StrikeCenter, FQuat::Identity, Objects, FCollisionShape::MakeSphere(EliteData.TelegraphRadius + 200));
    TSet<AActor*> Unique;
    for (const auto& Hit : Hits)
    {
        auto* Target = Hit.GetActor();
        if (!Target || Unique.Contains(Target) || !UPGAbilityBPLibrary::IsTargetActorHostile(Enemy, Target)) continue;
        const float Angle = EliteData.Pattern == EPGAttackPattern::Sweep ? EliteData.HalfAngleDegrees : 180.f;
        if (!PGAttackGeometry::Contains(Target->GetActorLocation(), StrikeCenter, PatternForward, EliteData.TelegraphRadius, Angle) ||
            FMath::Abs(Target->GetActorLocation().Z - StrikeCenter.Z) > 200) continue;
        // Walls block the same world-space area attack players see on the floor.
        FHitResult Wall; FCollisionQueryParams Params(SCENE_QUERY_STAT(PGPatternWall), false, Enemy); Params.AddIgnoredActor(Target);
        // World geometry blocks attacks; intervening enemies must not accidentally shield the player.
        if (GetWorld()->LineTraceSingleByObjectType(Wall, StrikeCenter + FVector(0,0,60), Target->GetActorLocation(),
            FCollisionObjectQueryParams(ECC_WorldStatic), Params)) continue;
        Unique.Add(Target);
        FGameplayEventData Event; Event.Instigator = Enemy; Event.Target = Target;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, PGGamePlayTags::Shared_Event_HitReact, Event);
        if (!IsActive()) return; // Damage can synchronously end the stage and cancel this ability.
    }
    if (auto* VFX = EliteData.SlamVFX.LoadSynchronous()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, StrikeCenter);
    UE_LOG(LogTemp, Log, TEXT("PGPattern Strike skill=%d pulse=%d hits=%d"), EliteData.SkillID, HazardIndex, Unique.Num());
    if (EliteData.Pattern == EPGAttackPattern::HazardSequence && ++HazardIndex < FMath::Clamp(EliteData.HazardCount,1,8))
    {
        // Fixed positions after aim lock: each following circle is visible for the entire interval.
        StrikeCenter = HazardOrigin + PatternForward * (EliteData.HazardSpacing * HazardIndex);
        ShowTelegraph(StrikeCenter, false);
        GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::StrikeElitePattern, FMath::Max(.1f, EliteData.HazardInterval), false);
        return;
    }
    BeginRecovery();
}

void UPGEnemyAbilityAttack::BeginRecovery()
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    Enemy->bPatternRecovering = true; Enemy->SetGuarding(false);
    Enemy->GetPGAbilitySystemComponent()->OpenRecoveryWindow(EliteData.RecoveryDuration, EliteData.RecoveryDamageBonus);
    FVector Floor = Enemy->GetActorLocation(); Floor.Z = PatternOrigin.Z;
    ShowTelegraph(Floor, false, true);
    GetWorld()->GetTimerManager().SetTimer(PatternTimer, this, &ThisClass::FinishElitePattern, FMath::Max(.05f, EliteData.RecoveryDuration), false);
    Enemy->PublishBossPresentation();
}
void UPGEnemyAbilityAttack::FinishElitePattern() { EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false); }
void UPGEnemyAbilityAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled)
{
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PatternTimer); GetWorld()->GetTimerManager().ClearTimer(UpdateTimer); }
    if (Telegraph) { Telegraph->DestroyComponent(); Telegraph = nullptr; }
    if (bCancelled && ActiveProjectile.IsValid()) ActiveProjectile->Destroy();
    ActiveProjectile.Reset();
    if (bElitePattern)
    {
        if (auto* Enemy = GetEnemyCharacterFromActorInfo())
        {
            if (auto* Anim = Enemy->GetMesh()->GetAnimInstance())
                if (auto* Montage = EliteData.ElitePresentationMontage.Get()) Anim->Montage_Stop(.1f, Montage);
            Enemy->ClearPatternHitboxes();
            Enemy->GetPGAbilitySystemComponent()->CloseRecoveryWindow();
            Enemy->bPatternActive = Enemy->bPatternRecovering = Enemy->bPatternStriking = Enemy->bPerformingHeavyAttack = false;
            Enemy->ActivePatternID = 0;
            if (Enemy->GetPGAbilitySystemComponent()->GetHealth() > 0) Enemy->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode));
            Enemy->PublishBossPresentation();
        }
        PatternTarget.Reset();
        bElitePattern = bTravelling = bStriking = false;
        UE_LOG(LogTemp, Log, TEXT("PGPattern End skill=%d cancelled=%d"), EliteData.SkillID, bCancelled);
    }
    Super::EndAbility(Handle, Info, Activation, bReplicate, bCancelled);
}

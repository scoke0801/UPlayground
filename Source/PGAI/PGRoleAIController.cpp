#include "PGRoleAIController.h"
#include "PGCombatDirectorSubsystem.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/Combat/PGEnemyAbilityAttack.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "HAL/IConsoleManager.h"
#include "PGCombatSpatial.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"

static TAutoConsoleVariable<int32> CVarPGUseCombatBT(TEXT("pg.AI.UseBehaviorTree"), 1,
    TEXT("Use role BTs for newly initialized enemies. 0 keeps the shared timer driver for comparison."));
static TAutoConsoleVariable<int32> CVarPGCombatPositionDebug(TEXT("pg.AI.DebugPositions"), 0,
    TEXT("Draw chosen combat positions and accepted local movement requests."));
static TAutoConsoleVariable<int32> CVarPGCombatDecisionDebug(TEXT("pg.AI.DebugDecisions"), 0,
    TEXT("Log combat eligibility, selection and execution reasons. Disabled by default."));
static TAutoConsoleVariable<int32> CVarPGCombatDecisionEnemy(TEXT("pg.AI.DebugEnemyID"), 0,
    TEXT("Limit decision logs to this enemy data ID; 0 includes all enemies."));

void APGRoleAIController::TraceCombatDecision(const TCHAR* Reason, int32 SkillID)
{
    if (!CVarPGCombatDecisionDebug.GetValueOnGameThread()) return;
    const auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    if (!Enemy || !GetWorld()) return;
    const int32 Filter = CVarPGCombatDecisionEnemy.GetValueOnGameThread();
    if (Filter != 0 && Filter != Enemy->GetCharacterTID()) return;
    // Bound repeated diagnostics to one entry per reason/skill/second per controller.
    const FString Key = FString::Printf(TEXT("%s:%d"), Reason, SkillID);
    const double Now = GetWorld()->GetTimeSeconds();
    if (const double* Next = NextDecisionTraceAt.Find(Key); Next && Now < *Next) return;
    NextDecisionTraceAt.Add(Key, Now + 1.0);
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Skill = Tables && SkillID ? Tables->GetRowData<FPGSkillDataRow>(SkillID) : nullptr;
    const auto* Target = CombatTarget.Get();
    UE_LOG(LogTemp, Log, TEXT("PGCombatDecision pawn=%s enemy=%d reason=%s skill=%d pending=%d phase=%d distance=%.1f pressure=%d montage=%s presentation=%s profile=%s"),
        *GetNameSafe(Enemy), Enemy->GetCharacterTID(), Reason, SkillID, PendingSkill, Enemy->BossPhase,
        Target ? FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation()) : -1.f,
        Skill ? Skill->AttackPressureCost : 0,
        Skill ? *Skill->MontagePath.ToString() : TEXT(""),
        Skill ? *Skill->ElitePresentationMontage.ToSoftObjectPath().ToString() : TEXT(""),
        Skill ? *Skill->EnemyProfile.ToSoftObjectPath().ToString() : TEXT(""));
}

APGRoleAIController::APGRoleAIController()
{
    SetGenericTeamId(FGenericTeamId(1));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void APGRoleAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Enemy && Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (!Data || Data->Mobility != EPGEnemyMobility::Flying || bHoldPosition || Enemy->bPatternActive ||
        !Enemy->GetPGAbilitySystemComponent() || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0) return;
    FVector Direction = bFlightApproach && CombatTarget.IsValid() ?
        (CombatTarget->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextFlightProbeAt)
    {
        NextFlightProbeAt = Now + .15;
        SafeFlightDirection = FVector::ZeroVector;
        const float Sign = Enemy->GetUniqueID() % 2 ? 1.f : -1.f;
        if (Data->bFlightOrbit && !bFlightApproach && PendingSkill == 0 && CombatTarget.IsValid())
        {
            const FVector ToTarget = CombatTarget->GetActorLocation() - Enemy->GetActorLocation();
            if (ToTarget.Size2D() < 650.f && ToTarget.Size2D() > 200.f)
                Direction = ToTarget.GetSafeNormal2D().RotateAngleAxis(70.f * Sign, FVector::UpVector);
        }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PGFlightCorridor), false, Enemy);
        const auto* Capsule = Enemy->GetCapsuleComponent();
        const auto Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
        const FVector Origin = Enemy->GetActorLocation();
        for (float Angle : {0.f, 45.f, -45.f, 90.f, -90.f})
        {
            const FVector Candidate = Direction.RotateAngleAxis(Angle * Sign, FVector::UpVector);
            const FVector Ahead = Origin + Candidate * 120.f;
            FHitResult Ground, Wall;
            if (!GetWorld()->LineTraceSingleByObjectType(Ground, Ahead + FVector(0,0,200), Ahead - FVector(0,0,700), FCollisionObjectQueryParams(ECC_WorldStatic), Query) || Ground.ImpactNormal.Z < .65f) continue;
            const float WantedZ = Ground.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + FMath::Clamp(Data->FlightHeight, 0.f, 300.f);
            if (FMath::Abs(WantedZ - Origin.Z) > FMath::Clamp(Data->FlightFloorTolerance, 30.f, 300.f)) continue;
            if (GetWorld()->SweepSingleByChannel(Wall, Origin, FVector(Ahead.X, Ahead.Y, WantedZ), FQuat::Identity, ECC_Pawn, Shape, Query)) continue;
            SafeFlightDirection = Candidate;
            break;
        }
    }
    Direction = SafeFlightDirection;
    if (Direction.IsNearlyZero()) Enemy->GetCharacterMovement()->StopMovementImmediately();
    const FVector Probe = Enemy->GetActorLocation() + Direction * 100.f;
    FHitResult Floor;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGFlightFloor), false, Enemy);
    if (!GetWorld()->LineTraceSingleByObjectType(Floor, Probe + FVector(0,0,300), Probe - FVector(0,0,1000),
        FCollisionObjectQueryParams(ECC_WorldStatic), Params))
    {
        Enemy->GetCharacterMovement()->StopMovementImmediately();
        TraceCombatDecision(TEXT("flight_floor_missing"));
        return;
    }
    const float Height = Floor.ImpactPoint.Z + Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() +
        FMath::Clamp(Data->FlightHeight, 0.f, 300.f);
    Direction.Z = FMath::Clamp((Height - Enemy->GetActorLocation().Z) / 80.f, -1.f, 1.f);
    Enemy->AddMovementInput(Direction.GetSafeNormal(), FMath::Min(1.f, Direction.Size()));
}
void APGRoleAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    PreviousSkill = 0;
    NextDecisionTraceAt.Reset();
    LastSkillUseAt.Reset(); SelectionChosenAt = 0; LastGuardAt = -1.e9;
    ObservedPlayerCast.Invalidate(); ReactionReadyAt = -1.e9; NextFlightProbeAt = 0; SafeFlightDirection = FVector::ZeroVector;
    EvadedPlayerCast.Invalidate(); NextEvadeAt = EvadeUntil = 0; LastEvadeAttackCount = ReactionMoveCount = 0;
    PendingSkill = 0;
    SequencePhase = 1;
    SequenceCursor = 0;
    RetreatUntil = NextRetreatAt = NextMoveRequestAt = 0;
    NextPositionAt = 0; bPositionMove = false; PositionMoveCount = 0;
    CombatTarget.Reset(); MoveTarget.Reset(); LastAcceptanceRadius = -1.f;
    SetCombatThinkingEnabled(true);
}
void APGRoleAIController::SetCombatThinkingEnabled(bool bEnabled)
{
    if (!bEnabled) { SetActorTickEnabled(false); bFlightApproach = false; EvadeUntil = 0; }
    GetWorldTimerManager().ClearTimer(ThinkTimer);
    if (bEnabled && GetPawn())
    {
        if (bUsingCombatBehaviorTree && CombatBehaviorTree)
        {
            RefreshCombatContext();
            RunBehaviorTree(CombatBehaviorTree);
            return;
        }
        GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, FMath::Max(.05f, DecisionInterval), true, .3f);
    }
    else
    {
        if (auto* BT = Cast<UBehaviorTreeComponent>(BrainComponent)) BT->StopTree(EBTStopMode::Forced);
        PendingSkill = 0;
        bPositionMove = false;
        ReleaseAttackReservation();
        StopMovement();
        if (auto* Enemy = Cast<APGCharacterEnemy>(GetPawn()))
            if (auto* ASC = Enemy->GetPGAbilitySystemComponent())
                if (const auto* Spec = ASC->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass())) ASC->CancelAbilityHandle(Spec->Handle);
    }
}
void APGRoleAIController::OnUnPossess()
{
    SetCombatThinkingEnabled(false);
    bUsingCombatBehaviorTree = false;
    CombatTarget.Reset(); MoveTarget.Reset();
    Super::OnUnPossess();
}
void APGRoleAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    SetCombatThinkingEnabled(false);
    Super::EndPlay(Reason);
}
bool APGRoleAIController::TryExecuteSkill(int32 SkillID)
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Tables = UPGDataTableManager::Get(this);
    if (!Enemy || !Tables || !Enemy->GetPGAbilitySystemComponent() || Enemy->bPatternActive || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        Enemy->IsBossTransitioning() || EvadeUntil > 0 || GetWorld()->GetTimeSeconds() < Enemy->NextCombatActionAt) return false;
    const auto* Data = Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID());
    const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(SkillID);
    if (!Data || !Data->SkillIdList.Contains(SkillID) || !Skill || !Skill->IsPatternValid() || Skill->MinimumBossPhase > Enemy->BossPhase ||
        !Enemy->GetSkillHandler() || !Enemy->GetSkillHandler()->IsSkillReadyByID(SkillID)) return false;
    if (Skill->Pattern == EPGAttackPattern::Summon && Enemy->GetLivingSummonCount() >= Skill->MaxLivingSummons) return false;
    if (const auto* Profile = Skill->EnemyProfile.LoadSynchronous())
        if (Profile->bGuardCounter && Enemy->CompletedAttackPatterns < Profile->AttacksBeforeGuard) return false;
    auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>();
    auto* Target = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0) return false;
    if (!Enemy->EnsureCombatReady(Target)) return false;
    if (!Skill->IsInActivationRange(FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation())) || !LineOfSightTo(Target)) return false;
    if (Director && !Director->TryReserve(Enemy, Target, Skill->AttackPressureCost))
    {
        TraceCombatDecision(TEXT("pressure_wait"), SkillID);
        return false;
    }
    auto* ASC = Enemy->GetPGAbilitySystemComponent();
    const auto Handle = GetAttackAbilityHandle();
    Enemy->RequestedSkillID = SkillID;
    bPositionMove = false;
    bFlightApproach = false;
    StopMovement();
    const bool bActivated = Handle.IsValid() && ASC->TryActivateAbility(Handle);
    Enemy->RequestedSkillID = 0;
    if ((!bActivated || !Enemy->bPatternActive) && Director) Director->Release(Enemy);
    if (bActivated && Enemy->bPatternActive)
    {
        PreviousSkill = SkillID;
        LastSkillUseAt.Add(SkillID, GetWorld()->GetTimeSeconds());
        if (const auto* Profile = Skill->EnemyProfile.Get(); Profile && Profile->bGuardCounter)
            LastGuardAt = GetWorld()->GetTimeSeconds();
        if (Enemy->BossPhase >= 2 && !Data->PhaseTwoSkillSequence.IsEmpty())
        {
            SequencePhase = Enemy->BossPhase;
            const int32 Index = Data->PhaseTwoSkillSequence.Find(SkillID);
            if (Index != INDEX_NONE) SequenceCursor = (Index + 1) % Data->PhaseTwoSkillSequence.Num();
        }
    }
    TraceCombatDecision(bActivated && Enemy->bPatternActive ? TEXT("started") : TEXT("activation_failed"), SkillID);
    return bActivated && Enemy->bPatternActive;
}

int32 APGRoleAIController::SelectSkill(const FPGEnemyDataRow& Data, const TArray<int32>& Candidates, int32 Phase,
    const TMap<int32, float>& Weights)
{
    if (SequencePhase != Phase) { SequencePhase = Phase; SequenceCursor = 0; }
    if (Candidates.IsEmpty()) return 0;
    if (Data.Role == EPGEnemyRole::Boss && Data.bPrioritizeGuard)
        if (auto* Tables = UPGDataTableManager::Get(this))
            for (int32 ID : Candidates)
                if (const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(ID))
                    if (const auto* Profile = Skill->EnemyProfile.LoadSynchronous(); Profile && Profile->bGuardCounter)
                    { TraceCombatDecision(TEXT("selected_guard_priority"), ID); return ID; }
    if (Data.Role == EPGEnemyRole::Boss && Phase >= 2 && !Data.PhaseTwoSkillSequence.IsEmpty())
        for (int32 Offset = 0; Offset < Data.PhaseTwoSkillSequence.Num(); ++Offset)
        {
            const int32 ID = Data.PhaseTwoSkillSequence[(SequenceCursor + Offset) % Data.PhaseTwoSkillSequence.Num()];
            if (Candidates.Contains(ID)) { TraceCombatDecision(TEXT("selected_phase_sequence"), ID); return ID; }
        }
    TArray<int32> Choices = Candidates;
    if (Choices.Num() > 1) Choices.Remove(PreviousSkill);
    float TotalWeight = 0.f;
    for (int32 ID : Choices) TotalWeight += Weights.Contains(ID) ? FMath::Max(0.f, Weights[ID]) : 1.f;
    if (TotalWeight <= 0.f) return 0;
    float Roll = FMath::FRand() * TotalWeight;
    for (int32 ID : Choices)
    {
        Roll -= Weights.Contains(ID) ? FMath::Max(0.f, Weights[ID]) : 1.f;
        if (Roll < 0.f) { TraceCombatDecision(TEXT("selected_weighted"), ID); return ID; }
    }
    TraceCombatDecision(TEXT("selected_weighted"), Choices.Last());
    return Choices.Last();
}
FGameplayAbilitySpecHandle APGRoleAIController::GetAttackAbilityHandle()
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* ASC = Enemy ? Enemy->GetPGAbilitySystemComponent() : nullptr;
    if (!ASC) return {};
    if (const auto* Spec = ASC->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass())) return Spec->Handle;
    return ASC->GiveAbility(FGameplayAbilitySpec(UPGEnemyAbilityAttack::StaticClass(), 1));
}

void APGRoleAIController::ReleaseAttackReservation()
{
    if (auto* World = GetWorld())
        if (auto* Director = World->GetSubsystem<UPGCombatDirectorSubsystem>()) Director->Release(Cast<APGCharacterEnemy>(GetPawn()));
}

void APGRoleAIController::PublishCombatBlackboard()
{
    if (!bUsingCombatBehaviorTree) return;
    if (auto* BB = GetBlackboardComponent())
    {
        BB->SetValueAsObject(TEXT("TargetActor"), CombatTarget.Get());
        auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
        // The running ability owns its chosen skill until its completion callback.
        if (!Enemy || !Enemy->bPatternActive) BB->SetValueAsInt(TEXT("SelectedSkillID"), PendingSkill);
    }
}

void APGRoleAIController::RefreshCombatContext()
{
    bFlightApproach = false;
    bHasReadySkill = false;
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    bHoldPosition = true;
    if (!Enemy || !Enemy->GetPGAbilitySystemComponent() || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        !Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0)
    {
        TraceCombatDecision(TEXT("invalid_combatant"));
        CombatTarget.Reset(); PendingSkill = 0;
        ReleaseAttackReservation();
        StopMovement();
        if (Enemy && Enemy->bPatternActive)
            if (const auto* Spec = Enemy->GetPGAbilitySystemComponent()->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass()))
                Enemy->GetPGAbilitySystemComponent()->CancelAbilityHandle(Spec->Handle);
        PublishCombatBlackboard();
        return;
    }
    if (CombatTarget.IsValid() && CombatTarget != Target) { PendingSkill = 0; ReleaseAttackReservation(); }
    CombatTarget = Target;
    if (EvadeUntil > 0)
    {
        if (GetWorld()->GetTimeSeconds() < EvadeUntil && GetMoveStatus() == EPathFollowingStatus::Moving)
        { PublishCombatBlackboard(); return; }
        EvadeUntil = 0; StopMovement();
        auto* Tables = UPGDataTableManager::Get(this);
        const auto* Data = Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
        Enemy->NextCombatActionAt = GetWorld()->GetTimeSeconds() + (Data ? FMath::Max(.2f, Data->EvadeRecovery) : .4f);
    }
    if (!Enemy->EnsureCombatReady(Target))
    {
        TraceCombatDecision(TEXT("entrance_locked"));
        PendingSkill = 0; ReleaseAttackReservation(); StopMovement(); PublishCombatBlackboard(); return;
    }
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    auto* Handler = Enemy->GetSkillHandler();
    if (!Data || !Handler)
    {
        TraceCombatDecision(TEXT("missing_combat_data"));
        PendingSkill = 0; ReleaseAttackReservation(); StopMovement(); PublishCombatBlackboard(); return;
    }
    SetActorTickEnabled(Data->Mobility == EPGEnemyMobility::Flying);
    const FVector Delta = Target->GetActorLocation() - Enemy->GetActorLocation();
    const float Distance = Delta.Size2D();
    const double Now = GetWorld()->GetTimeSeconds();
    // Observe during attacks/recovery so their completion does not restart every reaction delay.
    const bool bReactive = Data->bReactiveGuard || Data->bReactiveEvade;
    if (bReactive) bCanSeeTarget = LineOfSightTo(Target);
    if (bReactive && bCanSeeTarget && Distance <= FMath::Max(100.f, Data->ReactionRange))
    {
        const auto& Cast = Target->GetPlayerAttackComponent()->GetCastContext();
        if (Cast && Cast->CastId != ObservedPlayerCast)
        {
            ObservedPlayerCast = Cast->CastId;
            ReactionReadyAt = Now + FMath::Clamp(Data->ReactionDelay, .15f, 1.f);
        }
    }
    if (Enemy->bPatternActive || Enemy->IsBossTransitioning() || GetWorld()->GetTimeSeconds() < Enemy->NextCombatActionAt)
    {
        TraceCombatDecision(Enemy->bPatternActive ? TEXT("pattern_locked") :
            Enemy->IsBossTransitioning() ? TEXT("phase_transition") : TEXT("recovery_locked"));
        PendingSkill = 0; StopMovement(); PublishCombatBlackboard(); return;
    }
    if (!bReactive) bCanSeeTarget = LineOfSightTo(Target);
    const float TargetSide = FVector::DotProduct(Enemy->GetActorRightVector(), Delta.GetSafeNormal2D());
    Enemy->SetActorRotation(FMath::RInterpConstantTo(Enemy->GetActorRotation(), Delta.GetSafeNormal2D().Rotation(), FMath::Max(.05f, DecisionInterval), FMath::Max(1.f, Data->TurnSpeed)));
    Enemy->SetGuarding(Data->Role == EPGEnemyRole::Guardian);
    if (Now < RetreatUntil && GetMoveStatus() == EPathFollowingStatus::Moving) { PublishCombatBlackboard(); return; }
    if (RetreatUntil > 0) { StopMovement(); RetreatUntil = 0; }
    bHoldPosition = false;
    TArray<int32> Candidates;
    TMap<int32, float> Weights;
    float ReadyRange = 0.f;
    float NearestRange = TNumericLimits<float>::Max();
    if (TryReactiveEvade(*Data)) { bHoldPosition = true; PublishCombatBlackboard(); return; }
    for (int32 ID : Data->SkillIdList)
    {
        const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(ID);
        if (!Skill) { TraceCombatDecision(TEXT("missing_skill"), ID); continue; }
        if (Skill->MinimumBossPhase > Enemy->BossPhase) { TraceCombatDecision(TEXT("phase_required"), ID); continue; }
        if (Skill->SelectionWeight <= 0.f) { TraceCombatDecision(TEXT("weight_disabled"), ID); continue; }
        if (!Skill->IsPatternValid()) { TraceCombatDecision(TEXT("invalid_pattern"), ID); continue; }
        if (Skill->Pattern == EPGAttackPattern::Summon && Enemy->GetLivingSummonCount() >= Skill->MaxLivingSummons)
        { TraceCombatDecision(TEXT("summon_limit"), ID); continue; }
        if (const auto* Profile = Skill->EnemyProfile.LoadSynchronous())
        {
            if (Profile->bGuardCounter && Enemy->CompletedAttackPatterns < Profile->AttacksBeforeGuard)
            { TraceCombatDecision(TEXT("guard_attack_requirement"), ID); continue; }
            if (Profile->bGuardCounter && Now - LastGuardAt < Data->GuardRepeatCooldown)
            { TraceCombatDecision(TEXT("guard_repeat_cooldown"), ID); continue; }
            if (Profile->bGuardCounter && Data->bReactiveGuard && (Now < ReactionReadyAt || Now > ReactionReadyAt + .75))
            { TraceCombatDecision(TEXT("reaction_window"), ID); continue; }
        }
        const float Range = Skill->GetPatternActivationRange();
        NearestRange = FMath::Min(NearestRange, Range);
        if (!Handler->IsSkillReadyByID(ID)) { TraceCombatDecision(TEXT("cooldown"), ID); continue; }
        ReadyRange = FMath::Max(ReadyRange, Range);
        if (Skill->IsInActivationRange(Distance) && bCanSeeTarget)
        {
            Candidates.Add(ID);
            const double* LastUse = LastSkillUseAt.Find(ID);
            Weights.Add(ID, Skill->SelectionWeight * Skill->SelectionTuning.Evaluate(Distance, TargetSide, LastUse ? Now - *LastUse : 1.e9));
        }
        else TraceCombatDecision(!Skill->IsInActivationRange(Distance) ? TEXT("out_of_range") : TEXT("no_line_of_sight"), ID);
    }
    if (!Candidates.Contains(PendingSkill) || (Data->SelectionReconsiderSeconds > 0.f && Now - SelectionChosenAt >= FMath::Max(.5f, Data->SelectionReconsiderSeconds)))
    {
        PendingSkill = SelectSkill(*Data, Candidates, Enemy->BossPhase, Weights);
        SelectionChosenAt = Now;
        // Keep the FIFO place if another eligible skill replaces the previous choice.
        if (PendingSkill == 0) ReleaseAttackReservation();
    }
    else TraceCombatDecision(TEXT("selection_retained"), PendingSkill);
    bHasReadySkill = ReadyRange > 0.f;
    ApproachRange = (bHasReadySkill ? ReadyRange : NearestRange) * .8f;
    if (Data->PreferredDistance > 0.f) ApproachRange = FMath::Min(ApproachRange, Data->PreferredDistance);
    if (NearestRange == TNumericLimits<float>::Max()) ApproachRange = 180.f;
    if (!bHasReadySkill && Data->Mobility == EPGEnemyMobility::Ground && Data->Positioning.bRepositionWhileWaiting)
        ApproachRange = FMath::Max(ApproachRange, Data->Positioning.CooldownDistance);
    PublishCombatBlackboard();
}

bool APGRoleAIController::TryReactiveEvade(const FPGEnemyDataRow& Data)
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = CombatTarget.Get();
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Data.bReactiveEvade || Data.Mobility != EPGEnemyMobility::Ground || !Enemy || !Target ||
        !bCanSeeTarget || Now < NextEvadeAt || Now < ReactionReadyAt || Now > ReactionReadyAt + .5 ||
        !ObservedPlayerCast.IsValid() || EvadedPlayerCast == ObservedPlayerCast ||
        Enemy->CompletedAttackPatterns <= LastEvadeAttackCount ||
        FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation()) > Data.ReactionRange) return false;
    // Observe a real running attack, not input state or future hit geometry.
    const auto& Cast = Target->GetPlayerAttackComponent()->GetCastContext();
    if (!Cast || Cast->CastId != ObservedPlayerCast) return false;
    EvadedPlayerCast = ObservedPlayerCast;
    NextEvadeAt = Now + FMath::Max(2.f, Data.EvadeCooldown);
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!Nav) return false;
    const FVector Origin = Enemy->GetActorLocation();
    const FVector Radial = (Origin - Target->GetActorLocation()).GetSafeNormal2D();
    const float Sign = Enemy->GetUniqueID() % 2 ? 1.f : -1.f;
    const auto* Capsule = Enemy->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGReactiveEvade), false, Enemy);
    for (float Angle : {70.f, -70.f})
    {
        FNavLocation Point;
        if (!Nav->ProjectPointToNavigation(Origin + Radial.RotateAngleAxis(Angle * Sign, FVector::UpVector) *
            FMath::Clamp(Data.EvadeDistance, 60.f, 250.f), Point, FVector(50,50,100))) continue;
        const FVector Goal = Point.Location + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+2.f);
        if (FVector::Dist2D(Goal, Origin) < 60.f || FMath::Abs(Goal.Z-Origin.Z) > 45.f) continue;
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit, Origin, Goal, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params)) continue;
        if (MoveToLocation(Point.Location, 20.f, false, true, false, true, nullptr, false) != EPathFollowingRequestResult::RequestSuccessful) continue;
        EvadeUntil = Now + FMath::Clamp(Data.EvadeSeconds, .2f, .6f);
        LastEvadeAttackCount = Enemy->CompletedAttackPatterns;
        ++ReactionMoveCount;
        PendingSkill = 0; bPositionMove = false; MoveTarget.Reset(); ReleaseAttackReservation();
        TraceCombatDecision(TEXT("reactive_evade"));
        return true;
    }
    TraceCombatDecision(TEXT("reactive_evade_no_path"));
    return false;
}

bool APGRoleAIController::TryRetreat()
{
    if (bHoldPosition) return false;
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = CombatTarget.Get();
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Enemy && Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (!Data || !Target || Data->Mobility != EPGEnemyMobility::Ground || Data->Role != EPGEnemyRole::Shooter) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    const FVector Delta = Target->GetActorLocation() - Enemy->GetActorLocation();
    const float Distance = Delta.Size2D();
    if (Now < NextRetreatAt || Distance >= Data->PreferredDistance - Data->DistanceTolerance) return false;
    NextRetreatAt = Now + FMath::Max(.1f, Data->RetreatSeconds) + FMath::Max(.1f, Data->RetreatCooldown);
    if (auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        for (float Angle : {0.f, 60.f, -60.f, 100.f, -100.f})
        {
            const FVector Away = (-Delta.GetSafeNormal2D()).RotateAngleAxis(Angle, FVector::UpVector);
            FNavLocation Position;
            if (!Nav->ProjectPointToNavigation(Enemy->GetActorLocation() + Away * FMath::Max(1.f, Data->RetreatDistance), Position, FVector(100,100,200))) continue;
            // MoveTo rejects partial paths; do not run the same synchronous path search twice.
            if (FVector::Dist2D(Position.Location, Target->GetActorLocation()) <= Distance + 60) continue;
            if (MoveToLocation(Position.Location, 40.f, false, true, false, true, nullptr, false) == EPathFollowingRequestResult::Failed)
            { TraceCombatDecision(TEXT("retreat_request_failed")); continue; }
            RetreatUntil = Now + FMath::Max(.1f, Data->RetreatSeconds);
            MoveTarget.Reset();
            PendingSkill = 0; ReleaseAttackReservation(); PublishCombatBlackboard();
            return true;
        }
    }
    return false;
}

void APGRoleAIController::ApproachTarget()
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = CombatTarget.Get();
    if (!Enemy || !Target || bHoldPosition) return;
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (Data && Data->Mobility == EPGEnemyMobility::Stationary) { StopMovement(); return; }
    if (Data && Data->Mobility == EPGEnemyMobility::Flying)
    {
        bFlightApproach = PendingSkill == 0 && (!bCanSeeTarget || FVector::Dist2D(Target->GetActorLocation(), Enemy->GetActorLocation()) > ApproachRange);
        return;
    }
    if (TryPositionForCombat()) return;
    const float Distance = FVector::Dist2D(Target->GetActorLocation(), Enemy->GetActorLocation());
    // Waiting for pressure must not keep walking through the intended firing distance.
    if (PendingSkill != 0 || (bCanSeeTarget && Distance <= ApproachRange)) { StopMovement(); return; }
    const float Radius = bCanSeeTarget ? FMath::Max(30.f, ApproachRange - 50.f) : 30.f;
    if (MoveTarget == Target && GetMoveStatus() == EPathFollowingStatus::Moving && FMath::Abs(Radius - LastAcceptanceRadius) < 30.f) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextMoveRequestAt) return;
    NextMoveRequestAt = Now + FMath::Max(.1f, MoveRetryInterval);
    LastAcceptanceRadius = Radius; MoveTarget = Target;
    if (MoveToActor(Target, Radius, false, true, false, nullptr, false) == EPathFollowingRequestResult::Failed)
        TraceCombatDecision(TEXT("approach_request_failed"));
}

bool APGRoleAIController::TryPositionForCombat()
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = CombatTarget.Get();
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Enemy && Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (!Data || !Target) return false;
    const auto& Tuning = Data->Positioning;
    // A selected skill reaches this fallback only after execution/admission failed.
    const bool bWaiting = Tuning.bRepositionWhileWaiting && (!bHasReadySkill || PendingSkill != 0);
    const FVector Origin = Enemy->GetActorLocation(), Center = Target->GetActorLocation();
    const float MaxTravel = FMath::Clamp(Tuning.MaxMoveDistance, 60.f, 800.f);
    float Range = FMath::Max(80.f, ApproachRange);
    const auto* Skill = PendingSkill ? Tables->GetRowData<FPGSkillDataRow>(PendingSkill) : nullptr;
    if (Skill) Range = FMath::Clamp(Range, Skill->MinimumActivationRange + 20.f, FMath::Max(Skill->MinimumActivationRange + 20.f, Skill->GetPatternActivationRange() - 30.f));
    if (bPositionWaiting != bWaiting || FMath::Abs(PositionRange - Range) > 30.f)
    {
        if (bPositionMove) StopMovement();
        bPositionMove = false;
        NextPositionAt = 0;
        bPositionWaiting = bWaiting;
        PositionRange = Range;
    }
    if (!Tuning.bEnabled && !bWaiting) return false;
    if (FVector::Dist2D(Origin, Center) > Range + MaxTravel) { bPositionMove = false; return false; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextPositionAt) return bPositionMove && GetMoveStatus() == EPathFollowingStatus::Moving;
    NextPositionAt = Now + FMath::Max(.5f, Tuning.ReconsiderSeconds);
    bPositionMove = false;
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!Nav) return false;
    TArray<APGCharacterEnemy*> Allies;
    const float Separation = FMath::Clamp(Tuning.Separation, 60.f, 500.f);
    PGCombatSpatial::GatherNeighbors(Enemy, Origin, MaxTravel + Separation, Allies);
    TArray<FVector> Neighbors;
    for (const auto* Ally : Allies) Neighbors.Add(Ally->GetActorLocation());
    const float CurrentCost = PGCombatSpatial::PositionCost(Origin, Origin, Center, Range, Separation, Neighbors) + (bCanSeeTarget ? 0.f : 1000.f);
    struct FCandidate { FVector Location; float Cost; };
    TArray<FCandidate, TInlineAllocator<8>> Candidates;
    FVector Radial = (Origin - Center).GetSafeNormal2D();
    if (Radial.IsNearlyZero()) Radial = Enemy->GetActorForwardVector();
    const float Sign = (Enemy->GetUniqueID() % 2) ? 1.f : -1.f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGCombatPositionLOS), false, Enemy);
    Params.AddIgnoredActor(Target);
    for (float Angle : {0.f, 25.f, -25.f, 50.f, -50.f, 80.f, -80.f, 180.f})
    {
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Center + Radial.RotateAngleAxis(Angle * Sign, FVector::UpVector) * Range, Projected, FVector(80,80,200))) continue;
        const FVector Position = Projected.Location;
        const float Travel = FVector::Dist2D(Position, Origin);
        if (Travel < 60.f || Travel > MaxTravel || (Skill && !Skill->IsInActivationRange(FVector::Dist2D(Position, Center)))) continue;
        if (GetWorld()->LineTraceTestByChannel(Position + FVector(0,0,80), Center, ECC_Visibility, Params)) continue;
        const float Cost = PGCombatSpatial::PositionCost(Position, Origin, Center, Range, Separation, Neighbors);
        // While waiting, a short flank step is purposeful even in an uncrowded slot.
        // Never trade spacing for orbiting, or let the waiting policy affect ready pursuit.
        const bool bWaitStep = bWaiting && FMath::Abs(Angle) >= 25.f && FMath::Abs(Angle) <= 80.f &&
            PGCombatSpatial::IsWaitingPosition(Position, Center, Range, Separation, Neighbors);
        if (Cost + FMath::Max(10.f, Tuning.MinimumImprovement) < CurrentCost || bWaitStep)
            Candidates.Add({Position, Cost});
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Cost < B.Cost; });
    // At most two path requests per reconsideration; no duplicate preflight path search.
    for (int32 Index = 0; Index < FMath::Min(2, Candidates.Num()); ++Index)
    {
        if (MoveToLocation(Candidates[Index].Location, 35.f, false, true, false, true, nullptr, false) != EPathFollowingRequestResult::RequestSuccessful)
        { TraceCombatDecision(TEXT("position_request_failed")); continue; }
        PositionGoal = Candidates[Index].Location; bPositionMove = true; MoveTarget.Reset(); ++PositionMoveCount;
        if (bWaiting) TraceCombatDecision(bHasReadySkill ? TEXT("pressure_reposition") : TEXT("cooldown_reposition"), PendingSkill);
        if (CVarPGCombatPositionDebug.GetValueOnGameThread())
        {
            DrawDebugLine(GetWorld(), Origin, PositionGoal + FVector(0,0,20), FColor::Cyan, false, Tuning.ReconsiderSeconds);
            DrawDebugSphere(GetWorld(), PositionGoal, 35.f, 8, FColor::Cyan, false, Tuning.ReconsiderSeconds);
        }
        return true;
    }
    return false;
}

bool APGRoleAIController::StartCombatBehaviorTree()
{
    auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>();
    if (!Director) return false;
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Enemy && Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    CombatBehaviorTree = Data ? Data->CombatBehaviorTree.LoadSynchronous() : nullptr;
    if (!CombatBehaviorTree) CombatBehaviorTree = Director->GetCombatBehaviorTree(DecisionInterval);
    bUsingCombatBehaviorTree = true;
    if (!RunBehaviorTree(CombatBehaviorTree)) { bUsingCombatBehaviorTree = false; return false; }
    RefreshCombatContext();
    GetWorldTimerManager().ClearTimer(ThinkTimer);
    UE_LOG(LogTemp, Log, TEXT("PGCombatBT started pawn=%s"), *GetNameSafe(GetPawn()));
    return true;
}

void APGRoleAIController::Think()
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Enemy && Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (bUseCombatBehaviorTree && CVarPGUseCombatBT.GetValueOnGameThread() != 0 && Data && Data->Role != EPGEnemyRole::Legacy && StartCombatBehaviorTree()) return;
    if (BrainComponent && BrainComponent->IsRunning()) BrainComponent->StopLogic(TEXT("Timer combat role"));
    RefreshCombatContext();
    if (bHoldPosition || TryRetreat()) return;
    if (PendingSkill != 0)
    {
        if (TryExecuteSkill(PendingSkill)) PendingSkill = 0;
        else ApproachTarget();
    }
    else ApproachTarget();
}

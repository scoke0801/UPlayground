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

static TAutoConsoleVariable<int32> CVarPGUseCombatBT(TEXT("pg.AI.UseBehaviorTree"), 1,
    TEXT("Use role BTs for newly initialized enemies. 0 keeps the shared timer driver for comparison."));
static TAutoConsoleVariable<int32> CVarPGCombatPositionDebug(TEXT("pg.AI.DebugPositions"), 0,
    TEXT("Draw chosen combat positions and accepted local movement requests."));

APGRoleAIController::APGRoleAIController() { SetGenericTeamId(FGenericTeamId(1)); }
void APGRoleAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    PreviousSkill = 0;
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
        Enemy->IsBossTransitioning()) return false;
    const auto* Data = Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID());
    const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(SkillID);
    if (!Data || !Data->SkillIdList.Contains(SkillID) || !Skill || !Skill->IsPatternValid() || Skill->MinimumBossPhase > Enemy->BossPhase ||
        !Enemy->GetSkillHandler() || !Enemy->GetSkillHandler()->IsSkillReadyByID(SkillID)) return false;
    auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>();
    auto* Target = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0) return false;
    if (Director && !Director->TryReserve(Enemy, Target, Skill->AttackPressureCost)) return false;
    auto* ASC = Enemy->GetPGAbilitySystemComponent();
    const auto Handle = GetAttackAbilityHandle();
    Enemy->RequestedSkillID = SkillID;
    bPositionMove = false;
    StopMovement();
    const bool bActivated = Handle.IsValid() && ASC->TryActivateAbility(Handle);
    Enemy->RequestedSkillID = 0;
    if ((!bActivated || !Enemy->bPatternActive) && Director) Director->Release(Enemy);
    if (bActivated && Enemy->bPatternActive)
    {
        PreviousSkill = SkillID;
        if (Enemy->BossPhase >= 2 && !Data->PhaseTwoSkillSequence.IsEmpty())
        {
            SequencePhase = Enemy->BossPhase;
            const int32 Index = Data->PhaseTwoSkillSequence.Find(SkillID);
            if (Index != INDEX_NONE) SequenceCursor = (Index + 1) % Data->PhaseTwoSkillSequence.Num();
        }
    }
    return bActivated && Enemy->bPatternActive;
}

int32 APGRoleAIController::SelectSkill(const FPGEnemyDataRow& Data, const TArray<int32>& Candidates, int32 Phase,
    const TMap<int32, float>& Weights)
{
    if (SequencePhase != Phase) { SequencePhase = Phase; SequenceCursor = 0; }
    if (Candidates.IsEmpty()) return 0;
    if (Data.Role == EPGEnemyRole::Boss && Phase >= 2 && !Data.PhaseTwoSkillSequence.IsEmpty())
        for (int32 Offset = 0; Offset < Data.PhaseTwoSkillSequence.Num(); ++Offset)
        {
            const int32 ID = Data.PhaseTwoSkillSequence[(SequenceCursor + Offset) % Data.PhaseTwoSkillSequence.Num()];
            if (Candidates.Contains(ID)) return ID;
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
        if (Roll < 0.f) return ID;
    }
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
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    bHoldPosition = true;
    if (!Enemy || !Enemy->GetPGAbilitySystemComponent() || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        !Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0)
    {
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
    if (Enemy->bPatternActive || Enemy->IsBossTransitioning()) { PendingSkill = 0; StopMovement(); PublishCombatBlackboard(); return; }
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    auto* Handler = Enemy->GetSkillHandler();
    if (!Data || !Handler) { PendingSkill = 0; ReleaseAttackReservation(); StopMovement(); PublishCombatBlackboard(); return; }
    const FVector Delta = Target->GetActorLocation() - Enemy->GetActorLocation();
    const float Distance = Delta.Size2D();
    const double Now = GetWorld()->GetTimeSeconds();
    Enemy->SetActorRotation(FMath::RInterpConstantTo(Enemy->GetActorRotation(), Delta.Rotation(), FMath::Max(.05f, DecisionInterval), FMath::Max(1.f, Data->TurnSpeed)));
    Enemy->SetGuarding(Data->Role == EPGEnemyRole::Guardian);
    if (Now < RetreatUntil && GetMoveStatus() == EPathFollowingStatus::Moving) { PublishCombatBlackboard(); return; }
    if (RetreatUntil > 0) { StopMovement(); RetreatUntil = 0; }
    bHoldPosition = false;
    TArray<int32> Candidates;
    TMap<int32, float> Weights;
    float ReadyRange = 0.f;
    float NearestRange = TNumericLimits<float>::Max();
    bCanSeeTarget = LineOfSightTo(Target);
    for (int32 ID : Data->SkillIdList)
    {
        const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(ID);
        if (!Skill || Skill->MinimumBossPhase > Enemy->BossPhase || Skill->SelectionWeight <= 0.f || !Skill->IsPatternValid()) continue;
        const float Range = Skill->GetPatternActivationRange();
        NearestRange = FMath::Min(NearestRange, Range);
        if (!Handler->IsSkillReadyByID(ID)) continue;
        ReadyRange = FMath::Max(ReadyRange, Range);
        if (Skill->IsInActivationRange(Distance) && bCanSeeTarget) { Candidates.Add(ID); Weights.Add(ID, Skill->SelectionWeight); }
    }
    if (!Candidates.Contains(PendingSkill))
    {
        PendingSkill = SelectSkill(*Data, Candidates, Enemy->BossPhase, Weights);
        // Keep the FIFO place if another eligible skill replaces the previous choice.
        if (PendingSkill == 0) ReleaseAttackReservation();
    }
    ApproachRange = (ReadyRange > 0.f ? ReadyRange : NearestRange) * .8f;
    if (Data->PreferredDistance > 0.f) ApproachRange = FMath::Min(ApproachRange, Data->PreferredDistance);
    if (NearestRange == TNumericLimits<float>::Max()) ApproachRange = 180.f;
    PublishCombatBlackboard();
}

bool APGRoleAIController::TryRetreat()
{
    if (bHoldPosition) return false;
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = CombatTarget.Get();
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Enemy && Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (!Data || !Target || Data->Role != EPGEnemyRole::Shooter) return false;
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
            if (MoveToLocation(Position.Location, 40.f, false, true, false, true, nullptr, false) == EPathFollowingRequestResult::Failed) continue;
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
    MoveToActor(Target, Radius, false, true, false, nullptr, false);
}

bool APGRoleAIController::TryPositionForCombat()
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = CombatTarget.Get();
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Enemy && Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    if (!Data || !Target || !Data->Positioning.bEnabled) return false;
    const auto& Tuning = Data->Positioning;
    const FVector Origin = Enemy->GetActorLocation(), Center = Target->GetActorLocation();
    const float MaxTravel = FMath::Clamp(Tuning.MaxMoveDistance, 60.f, 800.f);
    float Range = FMath::Max(80.f, ApproachRange);
    const auto* Skill = PendingSkill ? Tables->GetRowData<FPGSkillDataRow>(PendingSkill) : nullptr;
    if (Skill) Range = FMath::Clamp(Range, Skill->MinimumActivationRange + 20.f, FMath::Max(Skill->MinimumActivationRange + 20.f, Skill->GetPatternActivationRange() - 30.f));
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
        if (Cost + FMath::Max(10.f, Tuning.MinimumImprovement) < CurrentCost) Candidates.Add({Position, Cost});
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Cost < B.Cost; });
    // At most two path requests per reconsideration; no duplicate preflight path search.
    for (int32 Index = 0; Index < FMath::Min(2, Candidates.Num()); ++Index)
    {
        if (MoveToLocation(Candidates[Index].Location, 35.f, false, true, false, true, nullptr, false) == EPathFollowingRequestResult::Failed) continue;
        PositionGoal = Candidates[Index].Location; bPositionMove = true; MoveTarget.Reset(); ++PositionMoveCount;
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

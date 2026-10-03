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
#include "NavigationPath.h"
#include "TimerManager.h"
#include "BrainComponent.h"
#include "Navigation/PathFollowingComponent.h"

APGRoleAIController::APGRoleAIController() { SetGenericTeamId(FGenericTeamId(1)); }
void APGRoleAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    PreviousSkill = 0;
    PendingSkill = 0;
    SequencePhase = 1;
    SequenceCursor = 0;
    NextDecision = RetreatUntil = NextRetreatAt = 0;
    SetCombatThinkingEnabled(true);
}
void APGRoleAIController::SetCombatThinkingEnabled(bool bEnabled)
{
    GetWorldTimerManager().ClearTimer(ThinkTimer);
    if (bEnabled && GetPawn()) GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, .2f, true, .3f);
    else
    {
        PendingSkill = 0;
        if (auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>()) Director->Release(Cast<APGCharacterEnemy>(GetPawn()));
        StopMovement();
        if (auto* Enemy = Cast<APGCharacterEnemy>(GetPawn()))
            if (auto* ASC = Enemy->GetPGAbilitySystemComponent())
                if (const auto* Spec = ASC->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass())) ASC->CancelAbilityHandle(Spec->Handle);
    }
}
void APGRoleAIController::OnUnPossess()
{
    SetCombatThinkingEnabled(false);
    Super::OnUnPossess();
}
void APGRoleAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>()) Director->Release(Cast<APGCharacterEnemy>(GetPawn()));
    GetWorldTimerManager().ClearTimer(ThinkTimer);
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
    auto* Spec = ASC->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass());
    if (!Spec)
    {
        ASC->GiveAbility(FGameplayAbilitySpec(UPGEnemyAbilityAttack::StaticClass(), 1));
        Spec = ASC->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass());
    }
    Enemy->RequestedSkillID = SkillID;
    StopMovement();
    const bool bActivated = Spec && ASC->TryActivateAbility(Spec->Handle);
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
void APGRoleAIController::Think()
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Enemy || !Enemy->GetPGAbilitySystemComponent() || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        !Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0)
    {
        PendingSkill = 0;
        if (auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>()) Director->Release(Enemy);
        StopMovement();
        if (Enemy && Enemy->bPatternActive)
            if (const auto* Spec = Enemy->GetPGAbilitySystemComponent()->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass()))
                Enemy->GetPGAbilitySystemComponent()->CancelAbilityHandle(Spec->Handle);
        return; // Keep the decision timer alive: a replacement player can be acquired later.
    }
    if (Enemy->bPatternActive || Enemy->IsBossTransitioning()) { PendingSkill = 0; StopMovement(); return; }
    if (BrainComponent && BrainComponent->IsRunning()) BrainComponent->StopLogic(TEXT("Data-driven role controller"));
    auto* Tables = UPGDataTableManager::Get(this);
    const auto* Data = Tables ? Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID()) : nullptr;
    auto* Handler = Enemy->GetSkillHandler();
    if (!Data || !Handler) { StopMovement(); return; }
    const FVector Delta = Target->GetActorLocation() - Enemy->GetActorLocation();
    const float Distance = Delta.Size2D();
    const double Now = GetWorld()->GetTimeSeconds();
    Enemy->SetActorRotation(FMath::RInterpConstantTo(Enemy->GetActorRotation(), Delta.Rotation(), .2f, FMath::Max(1.f, Data->TurnSpeed)));
    Enemy->SetGuarding(Data->Role == EPGEnemyRole::Guardian);
    if (Now < RetreatUntil && GetMoveStatus() == EPathFollowingStatus::Moving) return;
    // Try multiple connected retreat directions. A wall must not trap the shooter in an endless retreat task.
    if (Data->Role == EPGEnemyRole::Shooter && Now >= NextRetreatAt && Distance < Data->PreferredDistance - Data->DistanceTolerance)
    {
        NextRetreatAt = Now + FMath::Max(.1f, Data->RetreatSeconds) + FMath::Max(.1f, Data->RetreatCooldown);
        if (auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            for (float Angle : {0.f, 60.f, -60.f, 100.f, -100.f})
            {
                const FVector Away = (-Delta.GetSafeNormal2D()).RotateAngleAxis(Angle, FVector::UpVector);
                FNavLocation Position;
                if (!Nav->ProjectPointToNavigation(Enemy->GetActorLocation() + Away * FMath::Max(1.f, Data->RetreatDistance), Position, FVector(100,100,200))) continue;
                auto* Path = Nav->FindPathToLocationSynchronously(GetWorld(), Enemy->GetActorLocation(), Position.Location, Enemy);
                if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
                if (FVector::Dist2D(Position.Location, Target->GetActorLocation()) <= Distance + 60) continue;
                if (MoveToLocation(Position.Location, 40.f, false, true, false, true, nullptr, false) == EPathFollowingRequestResult::Failed) continue;
                RetreatUntil = Now + FMath::Max(.1f, Data->RetreatSeconds);
                return;
            }
        }
    }
    if (Now < NextDecision) return;
    TArray<int32> Candidates;
    TMap<int32, float> Weights;
    float ReadyRange = 0.f;
    float NearestRange = TNumericLimits<float>::Max();
    const bool bCanSeeTarget = LineOfSightTo(Target);
    for (int32 ID : Data->SkillIdList)
    {
        const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(ID);
        if (!Skill || Skill->MinimumBossPhase > Enemy->BossPhase || Skill->SelectionWeight <= 0.f || !Skill->IsPatternValid()) continue;
        const float Range = Skill->GetPatternActivationRange();
        NearestRange = FMath::Min(NearestRange, Range);
        if (!Handler->IsSkillReadyByID(ID)) continue;
        ReadyRange = FMath::Max(ReadyRange, Range);
        if (Skill->IsInActivationRange(Distance) && bCanSeeTarget)
        {
            Candidates.Add(ID);
            Weights.Add(ID, Skill->SelectionWeight);
        }
    }
    if (!Candidates.IsEmpty())
    {
        // Retain an eligible choice while queued; do not reroll away from a heavy attack.
        if (!Candidates.Contains(PendingSkill)) PendingSkill = SelectSkill(*Data, Candidates, Enemy->BossPhase, Weights);
        if (TryExecuteSkill(PendingSkill)) PendingSkill = 0;
        NextDecision = Now + .2;
    }
    else
    {
        PendingSkill = 0;
        if (auto* Director = GetWorld()->GetSubsystem<UPGCombatDirectorSubsystem>()) Director->Release(Enemy);
        // A cooling long-range skill must not stop an available short-range attack from approaching.
        float ApproachRange = (ReadyRange > 0.f ? ReadyRange : NearestRange) * .8f;
        if (Data->PreferredDistance > 0.f) ApproachRange = FMath::Min(ApproachRange, Data->PreferredDistance);
        if (NearestRange == TNumericLimits<float>::Max()) ApproachRange = 180.f;
        // Use a small acceptance radius when occluded so navigation can actually route around a wall.
        if (!bCanSeeTarget || Distance > ApproachRange)
            MoveToActor(Target, bCanSeeTarget ? FMath::Max(30.f, ApproachRange - 50.f) : 30.f, false, true, false, nullptr, false);
        else StopMovement();
    }
}

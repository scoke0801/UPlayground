#include "PGRoleAIController.h"
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
    NextDecision = RetreatUntil = NextRetreatAt = 0;
    SetCombatThinkingEnabled(true);
}
void APGRoleAIController::SetCombatThinkingEnabled(bool bEnabled)
{
    GetWorldTimerManager().ClearTimer(ThinkTimer);
    if (bEnabled && GetPawn()) GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, .2f, true, .3f);
    else
    {
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
    GetWorldTimerManager().ClearTimer(ThinkTimer);
    Super::EndPlay(Reason);
}
bool APGRoleAIController::TryExecuteSkill(int32 SkillID)
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Tables = UPGDataTableManager::Get(this);
    if (!Enemy || !Tables || Enemy->bPatternActive || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        GetWorld()->GetTimeSeconds() < Enemy->PhaseTransitionUntil) return false;
    const auto* Data = Tables->GetRowData<FPGEnemyDataRow>(Enemy->GetCharacterTID());
    const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(SkillID);
    if (!Data || !Data->SkillIdList.Contains(SkillID) || !Skill || Skill->MinimumBossPhase > Enemy->BossPhase ||
        !Enemy->GetSkillHandler() || !Enemy->GetSkillHandler()->IsSkillReadyByID(SkillID)) return false;
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
    return bActivated && Enemy->bPatternActive;
}
void APGRoleAIController::Think()
{
    auto* Enemy = Cast<APGCharacterEnemy>(GetPawn());
    auto* Target = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Enemy || !Enemy->GetPGAbilitySystemComponent() || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0 ||
        !Target || !Target->GetPGAbilitySystemComponent() || Target->GetPGAbilitySystemComponent()->GetHealth() <= 0)
    {
        StopMovement();
        if (Enemy && Enemy->bPatternActive)
            if (const auto* Spec = Enemy->GetPGAbilitySystemComponent()->FindAbilitySpecFromClass(UPGEnemyAbilityAttack::StaticClass()))
                Enemy->GetPGAbilitySystemComponent()->CancelAbilityHandle(Spec->Handle);
        return; // Keep the decision timer alive: a replacement player can be acquired later.
    }
    if (Enemy->bPatternActive || GetWorld()->GetTimeSeconds() < Enemy->PhaseTransitionUntil) { StopMovement(); return; }
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
    float ReadyRange = 0.f;
    float NearestRange = TNumericLimits<float>::Max();
    const bool bCanSeeTarget = LineOfSightTo(Target);
    for (int32 ID : Data->SkillIdList)
    {
        const auto* Skill = Tables->GetRowData<FPGSkillDataRow>(ID);
        if (!Skill || Skill->MinimumBossPhase > Enemy->BossPhase ||
            (Skill->TelegraphDuration > 0.f && !Skill->IsPatternValid())) continue;
        const float Range = Skill->GetPatternActivationRange();
        NearestRange = FMath::Min(NearestRange, Range);
        if (!Handler->IsSkillReadyByID(ID)) continue;
        ReadyRange = FMath::Max(ReadyRange, Range);
        if (Distance <= Range && bCanSeeTarget) Candidates.Add(ID);
    }
    if (!Candidates.IsEmpty())
    {
        if (Candidates.Num() > 1) Candidates.Remove(PreviousSkill);
        const int32 ID = Candidates[FMath::RandHelper(Candidates.Num())];
        if (TryExecuteSkill(ID)) PreviousSkill = ID;
        NextDecision = Now + .25;
    }
    else
    {
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

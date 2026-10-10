#include "PGStageManager.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/PGDataTableManager.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "PGShared/Shared/Structure/PGRunRandom.h"

bool APGStageManager::ConfigureDungeonCombat(const TArray<FVector>& Centers, float HalfSize,
    float EntryInset, float SpawnInset, float RecoverySeconds, int32 CombatSeed)
{
    if (bDungeonCombat || Centers.Num() != 6 || !FMath::IsFinite(HalfSize) ||
        !FMath::IsFinite(EntryInset) || !FMath::IsFinite(SpawnInset) || !FMath::IsFinite(RecoverySeconds) ||
        EntryInset < 200 || SpawnInset < 200 || HalfSize <= FMath::Max(EntryInset, SpawnInset) + 200 || RecoverySeconds < 1)
        return false;
    for (int32 I = 0; I < Centers.Num(); ++I)
    {
        if (Centers[I].ContainsNaN()) return false;
        for (int32 J = 0; J < I; ++J)
            if (FVector::Dist2D(Centers[I], Centers[J]) < HalfSize * 2) return false;
    }
    // Reuse authored combat budgets and reject a mismatched content set before resetting a run.
    const int32 Choices[] = {1, 2, 1, 2, 1};
    for (int32 Id = 1; Id <= 6; ++Id)
    {
        const auto* Row = PGData() ? PGData()->GetRowData<FPGStageDataRow>(Id) : nullptr;
        if (!Row || !IsValidStageData(*Row) || Row->bIsBossStage != (Id == 6) ||
            (Id <= 5 && (Row->RewardSelections != Choices[Id - 1] || Row->RewardPool.IsEmpty())) ||
            (Id == 6 && !Row->RewardPool.IsEmpty())) return false;
    }
    auto* Profile = UPGProfileSubsystem::Get(this);
    const int32 ProgressionSeed = FMath::Max(1, int32(uint32(PGRunRandom::Seed(CombatSeed,0,0,7)) & uint32(MAX_int32)));
    if (!Profile || Profile->IsSaveBlocked() || !Profile->BeginNewRun(ProgressionSeed)) return false;
    DungeonRooms = Centers;
    DungeonHalfSize = HalfSize;
    DungeonEntryInset = EntryInset;
    DungeonSpawnInset = SpawnInset;
    DungeonRecoverySeconds = RecoverySeconds;
    DungeonCombatSeed = CombatSeed;
    bDungeonCombat = true;
    SetActorTickEnabled(true);
    AwaitDungeonObjective(1);
    return true;
}

FVector APGStageManager::GetDungeonObjectiveLocation() const
{
    return DungeonRooms.IsValidIndex(CurrentStageId - 1) ? DungeonRooms[CurrentStageId - 1] : FVector::ZeroVector;
}

bool APGStageManager::IsInsideDungeonObjective(const FVector& Position, float Inset) const
{
    if (!DungeonRooms.IsValidIndex(CurrentStageId - 1)) return false;
    const FVector Delta = Position - GetDungeonObjectiveLocation();
    const float Extent = DungeonHalfSize - Inset;
    return Extent > 0 && FMath::Abs(Delta.X) <= Extent && FMath::Abs(Delta.Y) <= Extent &&
        Delta.Z >= -50 && Delta.Z < 400;
}

bool APGStageManager::CanStartDungeonObjective(int32 StageId) const
{
    const auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    return CurrentStageState == EPGStageState::DungeonTraversal && StageId == CurrentStageId &&
        Player && Player->GetPGAbilitySystemComponent() && Player->GetPGAbilitySystemComponent()->GetHealth() > 0 &&
        IsInsideDungeonObjective(Player->GetActorLocation(), DungeonEntryInset);
}

void APGStageManager::AwaitDungeonObjective(int32 StageId)
{
    CurrentStageId = StageId;
    CurrentStageState = EPGStageState::DungeonTraversal;
    CurrentWaveIndex = INDEX_NONE;
    ActiveWaves.Reset();
    DungeonUnreachableSince.Reset();
    UE_LOG(LogTemp, Log, TEXT("PGDungeon objective waiting=%d center=%s"), StageId, *GetDungeonObjectiveLocation().ToString());
}

void APGStageManager::StopDungeonCombat()
{
    CurrentStageState = EPGStageState::None;
    GetWorldTimerManager().ClearAllTimersForObject(this);
    RewardToken.Invalidate();
    CloseRewardWindow();
    ClearAllEnemies();
    SetActorTickEnabled(false);
    DungeonUnreachableSince.Reset();
}

void APGStageManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bDungeonCombat) return;
    if (CurrentStageState == EPGStageState::DungeonTraversal)
    {
        if (CanStartDungeonObjective(CurrentStageId)) StartStage(CurrentStageId);
        return;
    }
    if (CurrentStageState != EPGStageState::InProgress) return;
    auto* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!Player || !Nav) { FailStage(TEXT("Dungeon combat lost player/navigation.")); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto It = DungeonUnreachableSince.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !SpawnedEnemies.Contains(It.Key().Get())) It.RemoveCurrent();
    for (auto* Enemy : SpawnedEnemies)
    {
        if (!IsValid(Enemy)) { FailStage(TEXT("Dungeon enemy became invalid.")); return; }
        const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),
            Enemy->GetActorLocation(), Player->GetActorLocation(), Enemy);
        const bool Reachable = Enemy->GetActorLocation().Z >= GetDungeonObjectiveLocation().Z - 100 &&
            Path && Path->IsValid() && !Path->IsPartial();
        if (Reachable) DungeonUnreachableSince.Remove(Enemy);
        else
        {
            double* Since = DungeonUnreachableSince.Find(Enemy);
            if (!Since) DungeonUnreachableSince.Add(Enemy, Now);
            else if (Now - *Since >= DungeonRecoverySeconds)
            { FailStage(TEXT("Dungeon enemy unreachable; encounter cancelled without rewards.")); return; }
        }
    }
}

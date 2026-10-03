#include "PGCombatDirectorSubsystem.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPGAttackPressure(TEXT("pg.AI.MaxAttackPressure"), 3,
    TEXT("Per-target concurrent attack budget. Light attacks cost 1, elite attacks 2, boss attacks 3."));
static TAutoConsoleVariable<float> CVarPGAttackSpacing(TEXT("pg.AI.AttackStartSpacing"), .28f,
    TEXT("Minimum game-time seconds between enemy windups against one target."));

bool UPGCombatDirectorSubsystem::TryReserve(APGCharacterEnemy* Enemy, AActor* Target, int32 Cost)
{
    return TryReserveAt(Enemy, Target, Cost, GetWorld()->GetTimeSeconds(),
        FMath::Max(1, CVarPGAttackPressure.GetValueOnGameThread()), FMath::Max(0.f, CVarPGAttackSpacing.GetValueOnGameThread()));
}

bool UPGCombatDirectorSubsystem::TryReserveAt(APGCharacterEnemy* Enemy, AActor* Target, int32 Cost, double Now, int32 Budget, float Spacing)
{
    const auto Alive = [](const FRequest& Request)
    {
        return Request.Enemy.IsValid() && Request.Target.IsValid() &&
            Request.Enemy->GetPGAbilitySystemComponent()->GetHealth() > 0 && !Request.Enemy->IsBossTransitioning();
    };
    Active.RemoveAll([&](const FRequest& Request) { return !Alive(Request) || !Request.Enemy->bPatternActive; });
    Waiting.RemoveAll([&](const FRequest& Request) { return !Alive(Request) || Now - Request.LastSeen > .8; });
    for (auto It = NextStart.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || It.Value() < Now) It.RemoveCurrent();
    if (!IsValid(Enemy) || !IsValid(Target) || Cost <= 0 || Enemy->GetPGAbilitySystemComponent()->GetHealth() <= 0) return false;
    // Lowering the debug budget must never make an otherwise valid heavy attack impossible.
    Cost = FMath::Clamp(Cost, 1, FMath::Max(1, Budget));
    if (Active.ContainsByPredicate([&](const FRequest& Request) { return Request.Enemy == Enemy; })) return false;
    auto* Pending = Waiting.FindByPredicate([&](const FRequest& Request) { return Request.Enemy == Enemy; });
    if (Pending && Pending->Target != Target) { Release(Enemy); Pending = nullptr; }
    if (Pending) { Pending->Cost = Cost; Pending->LastSeen = Now; }
    else Waiting.Add({Enemy, Target, Cost, Now});
    const auto* First = Waiting.FindByPredicate([&](const FRequest& Request) { return Request.Target == Target; });
    if (!First || First->Enemy != Enemy || NextStart.FindRef(Target) > Now) return false;
    int32 Used = 0;
    for (const auto& Request : Active) if (Request.Target == Target) Used += Request.Cost;
    if (Used + Cost > Budget) return false;
    Active.Add({Enemy, Target, Cost, Now});
    Waiting.RemoveAll([&](const FRequest& Request) { return Request.Enemy == Enemy; });
    NextStart.Add(Target, Now + Spacing);
    return true;
}

void UPGCombatDirectorSubsystem::Release(APGCharacterEnemy* Enemy)
{
    Active.RemoveAll([&](const FRequest& Request) { return Request.Enemy == Enemy; });
    Waiting.RemoveAll([&](const FRequest& Request) { return Request.Enemy == Enemy; });
}

void UPGCombatDirectorSubsystem::Deinitialize()
{
    Active.Reset(); Waiting.Reset(); NextStart.Reset();
    Super::Deinitialize();
}

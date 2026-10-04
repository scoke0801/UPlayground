#include "PGCombatSpatial.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

void PGCombatSpatial::GatherNeighbors(APGCharacterEnemy* Enemy, const FVector& Center, float Radius, TArray<APGCharacterEnemy*>& Out)
{
    Out.Reset();
    if (!Enemy || !Enemy->GetWorld() || !FMath::IsFinite(Radius) || Radius <= 0.f) return;
    TArray<FOverlapResult> Hits;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGCombatNeighbors), false, Enemy);
    Enemy->GetWorld()->OverlapMultiByObjectType(Hits, Center, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);
    for (const auto& Hit : Hits)
    {
        auto* Ally = Cast<APGCharacterEnemy>(Hit.GetActor());
        const auto* ASC = IsValid(Ally) ? Ally->GetPGAbilitySystemComponent() : nullptr;
        if (Ally != Enemy && ASC && ASC->GetHealth() > 0.f && FVector::DistSquared(Center, Ally->GetActorLocation()) <= FMath::Square(Radius))
            Out.AddUnique(Ally);
    }
}

APGCharacterEnemy* PGCombatSpatial::FindInjuredAlly(APGCharacterEnemy* Enemy, float Radius, float HealthThreshold, bool bIncludeSelf)
{
    if (!Enemy) return nullptr;
    TArray<APGCharacterEnemy*> Neighbors;
    GatherNeighbors(Enemy, Enemy->GetActorLocation(), Radius, Neighbors);
    if (bIncludeSelf) Neighbors.Add(Enemy);
    APGCharacterEnemy* Best = nullptr;
    float BestRatio = FMath::Clamp(HealthThreshold, 0.f, 1.f);
    for (auto* Ally : Neighbors)
    {
        const auto* ASC = Ally->GetPGAbilitySystemComponent();
        const auto* Stats = Ally->GetEnemyStatComponent();
        if (!ASC || ASC->GetHealth() <= 0.f || !Stats) continue;
        const float Ratio = Stats->GetHealthRatio();
        if (Ratio <= BestRatio) { Best = Ally; BestRatio = Ratio; }
    }
    return Best;
}

float PGCombatSpatial::PositionCost(const FVector& Candidate, const FVector& Origin, const FVector& Target,
    float DesiredRange, float Separation, TConstArrayView<FVector> Neighbors)
{
    float Cost = .6f * FMath::Abs(FVector::Dist2D(Candidate, Target) - DesiredRange) + .12f * FVector::Dist2D(Candidate, Origin);
    for (const auto& Neighbor : Neighbors) Cost += 4.f * FMath::Max(0.f, Separation - FVector::Dist2D(Candidate, Neighbor));
    return Cost;
}

#pragma once
#include "CoreMinimal.h"
class APGCharacterEnemy;

/** Local collision broadphase queries, shared by support selection and combat spacing. */
namespace PGCombatSpatial
{
    PGAI_API void GatherNeighbors(APGCharacterEnemy* Enemy, const FVector& Center, float Radius, TArray<APGCharacterEnemy*>& Out);
    PGAI_API APGCharacterEnemy* FindInjuredAlly(APGCharacterEnemy* Enemy, float Radius, float HealthThreshold, bool bIncludeSelf);
    // Lower is better. Travel cost and an improvement threshold prevent aimless orbiting.
    PGAI_API float PositionCost(const FVector& Candidate, const FVector& Origin, const FVector& Target,
        float DesiredRange, float Separation, TConstArrayView<FVector> Neighbors);
}

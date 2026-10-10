#pragma once
#include "CoreMinimal.h"
#include "PGAttackPattern.generated.h"

UENUM(BlueprintType)
// Append only: existing DataTables serialize these values.
enum class EPGAttackPattern : uint8 { LegacySlam, Sweep, ChargeSlam, HazardSequence, AimedProjectile, Thrust, RingBurst, Summon };

UENUM(BlueprintType)
enum class EPGEnemyMobility : uint8 { Ground, Flying, Stationary };

UENUM(BlueprintType)
enum class EPGEnemyRole : uint8 { Legacy, Chaser, Shooter, Guardian, Crusher, Warden, Boss };

/** Shared world-space geometry for telegraphs and damage. Z tolerance is evaluated by the caller. */
namespace PGAttackGeometry
{
    inline bool Contains(const FVector& Point, const FVector& Origin, const FVector& Forward,
        float Radius, float HalfAngleDegrees = 180.f)
    {
        const FVector Delta = (Point - Origin) * FVector(1, 1, 0);
        if (Delta.SizeSquared() > FMath::Square(FMath::Max(0.f, Radius))) return false;
        return Delta.IsNearlyZero() || FVector::DotProduct(Delta.GetSafeNormal(), Forward.GetSafeNormal2D()) + KINDA_SMALL_NUMBER >=
            FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDegrees, 0.f, 180.f)));
    }
    inline bool InLine(const FVector& Point, const FVector& Origin, const FVector& Forward, float Length, float HalfWidth)
    {
        const FVector Delta = Point - Origin, F = Forward.GetSafeNormal2D();
        const float Along = FVector::DotProduct(Delta, F);
        return Along >= 0 && Along <= Length && FMath::Abs(Delta.X * F.Y - Delta.Y * F.X) <= HalfWidth;
    }

    inline bool InRing(const FVector& Point, const FVector& Origin, float InnerRadius, float OuterRadius)
    {
        const float DistanceSquared = FVector::DistSquared2D(Point, Origin);
        return DistanceSquared >= FMath::Square(InnerRadius) && DistanceSquared <= FMath::Square(OuterRadius);
    }

    inline float VolleyAngle(int32 Index, int32 Count, float HalfAngle)
    {
        return Count <= 1 ? 0.f : FMath::Lerp(-HalfAngle, HalfAngle, float(Index) / float(Count - 1));
    }
}

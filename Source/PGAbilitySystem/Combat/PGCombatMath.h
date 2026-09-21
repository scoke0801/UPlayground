#pragma once
#include "CoreMinimal.h"
namespace PGCombatMath
{
inline float Damage(float Attack, float Defense, bool bCritical, float CritBonusPercent,
    float DefenseConstant, float BaseCritMultiplier, float MinimumDamage)
{
    if (!FMath::IsFinite(Attack) || !FMath::IsFinite(Defense) || !FMath::IsFinite(CritBonusPercent) ||
        !FMath::IsFinite(DefenseConstant) || !FMath::IsFinite(BaseCritMultiplier) || !FMath::IsFinite(MinimumDamage)) return 0.f;
    const float Constant = FMath::Max(1.f, DefenseConstant);
    const float Reduced = FMath::Max(0.f, Attack) * Constant / (FMath::Max(0.f, Defense) + Constant);
    return FMath::Max(FMath::Max(0.f, MinimumDamage), Reduced *
        (bCritical ? FMath::Max(1.f, BaseCritMultiplier + CritBonusPercent / 100.f) : 1.f));
}
}
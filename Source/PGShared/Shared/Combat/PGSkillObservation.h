#pragma once

#include "CoreMinimal.h"
#include "PGSkillCastContext.h"

// Optional QA state only. Never used to authorize hits, procs, cooldowns or input.
struct FPGSkillObservation
{
    FGuid CastId = FGuid::NewGuid();
    int32 SkillID = 0;
    double InputAt = -1.; // -1 means held repeat / programmatic activation, not a fresh press.
    double Started = 0.;
    FVector Origin = FVector::ZeroVector;
    TSharedPtr<FPGSkillCastContext> ProfileCast;
    int32 Hits = 0;
    float DirectDamage = 0.f;
    bool bEnded = false;
};

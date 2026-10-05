#pragma once
#include "CoreMinimal.h"

// Small read-only snapshot of the player and their most recently hit living target.
struct FPGBuildCombatState
{
    int32 FrenzyStacks = 0;
    int32 FrenzyMaxStacks = 0;
    float FrenzySeconds = 0;
    int32 BleedStacks = 0;
    float BleedSeconds = 0;
    int32 ShockHits = 0;
    int32 ShockHitsRequired = 0;
    float WeaknessSeconds = 0;
    FString TargetName;
    bool bShockProc = false;
    bool bRefundProc = false;
    int32 RefundSkillID = 0;
    bool bAfterimageProc = false;
};

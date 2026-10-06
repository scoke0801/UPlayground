#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"

enum class EPGBossCombatState : uint8 { Preparing, Windup, Attacking, Recovery, Transition, Defeated, Hidden, Guard };

/** Value snapshot: UI does not own combat state or require a particular stage number. */
struct FPGSharedBossPresentation : IPGEventData
{
    TWeakObjectPtr<AActor> Owner;
    FText Name;
    FText Attack;
    FText TransitionText;
    float HealthRatio = 1.f;
    int32 Phase = 1;
    EPGBossCombatState State = EPGBossCombatState::Preparing;
    float DefeatDisplaySeconds = 3.f;
};

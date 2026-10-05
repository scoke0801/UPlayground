#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"

struct FPGConsumableState
{
    int32 Count = 0;
    int32 Capacity = 0;
    float Cooldown = 0.f;
    float CooldownDuration = 0.f;
    float HealFraction = 0.f;
    float LowHealthFraction = .35f;
    bool bCanUse = false;
    FText Name;
    FText Reason;
    FText Notice;
};

struct FPGConsumablePresentation : IPGEventData
{
    TWeakObjectPtr<AActor> Owner;
    FPGConsumableState State;
};

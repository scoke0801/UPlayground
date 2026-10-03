#pragma once
#include "CoreMinimal.h"
#include "PGInventoryTypes.h"

/** Read-only presentation snapshot. Never used to grant or reconstruct rewards. */
struct FPGRunResultView
{
    bool bSavePending = false;
    int32 CompletedStages = 0;
    int32 CompletedRuns = 0;
    TMap<int32, int32> SelectedRewards;
    FPGItemInstance Loot;
};

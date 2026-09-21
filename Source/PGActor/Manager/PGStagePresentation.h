#pragma once
#include "CoreMinimal.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"

// Gameplay owns choices and commit callbacks; the presentation consumer owns widgets/input mode.
DECLARE_DELEGATE_RetVal_TwoParams(bool, FPGStageSubmit, FGuid, int32);
struct FPGStagePresentation : IPGEventData
{
    TWeakObjectPtr<AActor> Owner;
    FGuid Token;
    TArray<FPGStageReward> Choices;
    FText Status;
    bool bClose = false;
    FPGStageSubmit Submit;
    FSimpleDelegate Retry;
};

#pragma once
#include "CoreMinimal.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"
#include "PGShared/Shared/Structure/PGRunResultView.h"

// Gameplay owns choices and commit callbacks; the presentation consumer owns widgets/input mode.
DECLARE_DELEGATE_RetVal_TwoParams(bool, FPGStageSubmit, FGuid, int32);
struct FPGStagePresentation : IPGEventData
{
    TWeakObjectPtr<AActor> Owner;
    FGuid Token;
    TArray<FPGStageReward> Choices;
    FText Status;
    FText ActionLabel;
    bool bRunResult = false;
    FPGRunResultView Result;
    bool bClose = false;
    FPGStageSubmit Submit;
    FSimpleDelegate Retry;
};

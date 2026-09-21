#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/Validation/PGStageValidation.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGStageValidationTest, "PG.Stage.DataValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGStageValidationTest::RunTest(const FString& Parameters)
{
    FPGStageDataRow Data;
    Data.Id = 1;
    Data.MonsterSpawnInfos.Add(FPGMonsterSpawnInfo(1, 2, 0, 2.f));
    Data.SpawnInterval = 0.f;
    FString Error;
    TestTrue(TEXT("Zero interval with delayed entry is supported"), PGStageValidation::Validate(Data, Error));
    Data.SpawnBatchSize = 0;
    TestFalse(TEXT("Zero batch cannot make progress"), PGStageValidation::Validate(Data, Error));
    Data.SpawnBatchSize = 1;
    Data.MonsterSpawnInfos[0].SpawnDelayTime = -1.f;
    TestFalse(TEXT("Negative delay"), PGStageValidation::Validate(Data, Error));
    Data.MonsterSpawnInfos[0].SpawnDelayTime = 0.f;
    Data.MonsterSpawnInfos[0].SpawnCount = MAX_int32;
    Data.MonsterSpawnInfos.Add(FPGMonsterSpawnInfo(2, 1));
    TestFalse(TEXT("Count overflow"), PGStageValidation::Validate(Data, Error));
    return true;
}
#endif
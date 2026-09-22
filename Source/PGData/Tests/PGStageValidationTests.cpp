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
    Data.Waves.AddDefaulted();
    TestFalse(TEXT("Empty authored wave is invalid"), PGStageValidation::Validate(Data, Error));
    Data.Waves[0].MonsterSpawnInfos.Add(FPGMonsterSpawnInfo(1, 2));
    TestTrue(TEXT("Authored waves take precedence over legacy spawns"), PGStageValidation::Validate(Data, Error));
    Data.Waves[0].StartDelay = -1.f;
    TestFalse(TEXT("Negative intermission is invalid"), PGStageValidation::Validate(Data, Error));
    Data.Waves[0].StartDelay = 0.f;
    Data.BuildDuration = -1.f;
    TestFalse(TEXT("Negative build duration is invalid"), PGStageValidation::Validate(Data, Error));
    Data.BuildDuration = 0.f;
    TestTrue(TEXT("Immediate wave and zero build time are supported"), PGStageValidation::Validate(Data, Error));
    Data.Waves[0].MonsterSpawnInfos[0].SpawnCount = MAX_int32;
    FPGStageWave Second;
    Second.MonsterSpawnInfos.Add(FPGMonsterSpawnInfo(1, 1));
    Data.Waves.Add(Second);
    TestFalse(TEXT("Total count overflow across waves"), PGStageValidation::Validate(Data, Error));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGAI/PGRoleAIController.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGBossSelectionTest, "PG.Content.BossAttackSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGBossSelectionTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* AI = World->SpawnActor<APGRoleAIController>();
    FPGEnemyDataRow Data; Data.Role = EPGEnemyRole::Boss; Data.PhaseTwoSkillSequence = {15108,15107,15106};
    AI->PreviousSkill = 15106;
    TestEqual(TEXT("Phase one alternates available attacks"), AI->SelectSkill(Data, {15106,15107}, 1), 15107);
    TestEqual(TEXT("Phase two starts with its new attack"), AI->SelectSkill(Data, {15106,15107,15108}, 2), 15108);
    AI->SequenceCursor = 1;
    TestEqual(TEXT("Combination follows with charge"), AI->SelectSkill(Data, {15106,15107,15108}, 2), 15107);
    TestEqual(TEXT("Cooling charge falls through to sweep"), AI->SelectSkill(Data, {15106,15108}, 2), 15106);
    TestEqual(TEXT("Distant target can still receive available wave"), AI->SelectSkill(Data, {15108}, 2), 15108);
    TestEqual(TEXT("All cooldowns produce no attack"), AI->SelectSkill(Data, {}, 2), 0);
    Data.PhaseTwoSkillSequence = {999};
    TestEqual(TEXT("Unavailable authored combo falls back to valid attack"), AI->SelectSkill(Data, {15107}, 2), 15107);
    Data.Role = EPGEnemyRole::Shooter; AI->PreviousSkill = 0;
    const TMap<int32, float> Weights = {{15102,0.f},{15112,3.f}};
    for (int32 Index = 0; Index < 20; ++Index)
        TestEqual(TEXT("Disabled weight never steals the contextual attack"), AI->SelectSkill(Data, {15102,15112}, 1, Weights), 15112);
    World->DestroyWorld(false);
    return true;
}
#endif

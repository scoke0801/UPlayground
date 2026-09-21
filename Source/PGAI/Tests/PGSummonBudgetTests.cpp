#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGAI/Service/PGBTService_SelectSkill.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGSummonBudgetTest, "PG.AI.SummonBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGSummonBudgetTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    Enemy->GetSkillHandler()->AddSkill(EPGSkillSlot::NormalAttack, 999);
    auto* Skill = Enemy->GetSkillHandler()->GetSkillData(EPGSkillSlot::NormalAttack);
    Skill->SkillId = 999; Skill->Priority = 100;
    auto* Board = NewObject<UBlackboardData>();
    FBlackboardEntry Entry; Entry.EntryName = TEXT("SummonCount"); Entry.KeyType = NewObject<UBlackboardKeyType_Int>(Board); Board->Keys.Add(Entry);
    auto* BB = NewObject<UBlackboardComponent>(Enemy); TestTrue(TEXT("Blackboard initialized"), BB->InitializeBlackboard(*Board));
    auto* Service = NewObject<UPGBTService_SelectSkill>();
    TSet<EPGSkillType> Types; Types.Add(EPGSkillType::SummonEnemy);
    BB->SetValueAsInt(TEXT("SummonCount"), Service->MaxSummonCountPerBattle);
    TestEqual(TEXT("Escalated priority cannot bypass summon limit"), Service->CalculateSkillPriority(999, EPGSkillType::SummonEnemy, 100, 1, Enemy, BB, Types), 0.f);
    BB->SetValueAsInt(TEXT("SummonCount"), 0);
    TestTrue(TEXT("Summon below limit remains eligible"), Service->CalculateSkillPriority(999, EPGSkillType::SummonEnemy, 100, 1, Enemy, BB, Types) > 0);
    World->DestroyWorld(false); return true;
}
#endif

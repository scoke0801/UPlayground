#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGUI/Widget/Window/PGUIWindowRewardSelect.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRewardPresentationTest, "PG.CombatCycle.RewardSubmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGRewardPresentationTest::RunTest(const FString& Parameters)
{
    auto* Window = NewObject<UPGUIWindowRewardSelect>();
    const FGuid Token = FGuid::NewGuid();
    Window->SetChoices(Token, {});
    TestFalse(TEXT("Missing consumer cannot consume token"), Window->SubmitChoice(INDEX_NONE));
    int32 Calls=0; bool Allow=false;
    Window->OnSubmit.BindLambda([&](FGuid Submitted, int32 Index)
    {
        ++Calls; TestEqual(TEXT("Original stage token preserved"),Submitted,Token);
        TestEqual(TEXT("Empty pool continues with INDEX_NONE"),Index,INDEX_NONE); return Allow;
    });
    TestFalse(TEXT("Failed grant remains retryable"),Window->SubmitChoice(INDEX_NONE));
    Allow=true;
    TestTrue(TEXT("Retry succeeds"),Window->SubmitChoice(INDEX_NONE));
    TestFalse(TEXT("Repeated input cannot pay twice"),Window->SubmitChoice(INDEX_NONE));
    TestEqual(TEXT("Exactly two grant attempts"),Calls,2);
    return true;
}
#endif

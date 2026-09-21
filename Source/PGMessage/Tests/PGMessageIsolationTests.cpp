#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGMessageIsolationTest, "PG.Lifecycle.MessageIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGMessageIsolationTest::RunTest(const FString& Parameters)
{
    auto* A = NewObject<UPGMessageManager>(NewObject<UGameInstance>());
    auto* B = NewObject<UPGMessageManager>(NewObject<UGameInstance>());
    int32 CountA = 0, CountB = 0;
    const auto HandleB = B->RegisterDelegate(EPGUIMessageType::StagePresentation, FPGMessageDelegate::FDelegate::CreateLambda([&](const IPGEventData*){ ++CountB; }));
    for (int32 Cycle = 0; Cycle < 20; ++Cycle)
    {
        const auto HandleA = A->RegisterDelegate(EPGUIMessageType::StagePresentation, FPGMessageDelegate::FDelegate::CreateLambda([&](const IPGEventData*){ ++CountA; }));
        A->SendMessage(EPGUIMessageType::StagePresentation, nullptr);
        A->UnregisterDelegate(EPGUIMessageType::StagePresentation, HandleA);
        A->SendMessage(EPGUIMessageType::StagePresentation, nullptr);
        TestEqual(TEXT("No subscription accumulation"), CountA, Cycle + 1);
        TestEqual(TEXT("Other instance receives nothing"), CountB, 0);
    }
    A->ClearAllDelegates(); B->SendMessage(EPGUIMessageType::StagePresentation, nullptr);
    TestEqual(TEXT("Other instance survives cleanup"), CountB, 1);
    B->UnregisterDelegate(EPGUIMessageType::StagePresentation, HandleB);
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGPlayerSkillHandler.h"
#include "PGData/DataAsset/Input/PGQuarterViewData.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGComboSequenceTest, "PG.Combat.ComboSequence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGComboSequenceTest::RunTest(const FString&)
{
    FPGPlayerSkillHandler Handler;
    FPGSkillDataRow Normal;
    Normal.SkillID = 100;
    Normal.ChainSkillIdList = {101, 102};
    Normal.ComboResetSeconds = .4f;
    const auto Slot = EPGSkillSlot::NormalAttack;
    TestEqual(TEXT("New combat starts at first strike"), Handler.GetComboIndex(Slot, Normal, 0.), 0);
    Handler.AdvanceCombo(Slot, Normal, 0., 1.f);
    TestEqual(TEXT("A prompt second input advances"), Handler.GetComboIndex(Slot, Normal, .8), 1);
    Handler.AdvanceCombo(Slot, Normal, .8, .8f);
    TestEqual(TEXT("Third input reaches the finisher"), Handler.GetComboIndex(Slot, Normal, 1.4), 2);
    Handler.AdvanceCombo(Slot, Normal, 1.4, 1.f);
    TestEqual(TEXT("Finisher loops to first strike"), Handler.GetComboIndex(Slot, Normal, 2.2), 0);
    Handler.AdvanceCombo(Slot, Normal, 2.2, 2.f);
    TestEqual(TEXT("Long attacks retain the chain until animation finishes"), Handler.GetComboIndex(Slot, Normal, 4.4), 1);
    TestEqual(TEXT("Idle time resets the chain"), Handler.GetComboIndex(Slot, Normal, 5.), 0);
    Handler.AdvanceCombo(Slot, Normal, 5., .5f);
    TestEqual(TEXT("First strike after idle starts a new chain"), Handler.GetComboIndex(Slot, Normal, 5.2), 1);
    FPGSkillDataRow Other = Normal;
    Other.SkillID = 110;
    TestEqual(TEXT("Another slot cannot borrow a combo"), Handler.GetComboIndex(EPGSkillSlot::SkillSlot_1, Other, 5.2), 0);
    TestEqual(TEXT("Replacing the skill in the same slot resets its chain"), Handler.GetComboIndex(Slot, Other, 5.2), 0);
    Handler.AdvanceCombo(EPGSkillSlot::SkillSlot_Roll, Other, 5.2, .6f);
    TestEqual(TEXT("A committed dodge breaks the attack chain"), Handler.GetComboIndex(Slot, Normal, 5.4), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGDodgeDirectionTest, "PG.Combat.DodgeDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGDodgeDirectionTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    Player->LastAimDirection = FVector::ForwardVector;
    Player->MoveInputDirection = FVector::RightVector;
    Player->FaceDodgeDirection();
    TestTrue(TEXT("Dodge follows movement even while aiming elsewhere"), Player->GetActorForwardVector().Equals(FVector::RightVector, .001f));
    TestTrue(TEXT("Motion warping uses the same direction as facing"), Player->GetDodgeDirection().Equals(Player->GetActorForwardVector(), .001f));
    Player->Input_MoveReleased(FInputActionValue(FVector2D::ZeroVector));
    Player->FaceDodgeDirection();
    TestTrue(TEXT("Released movement falls back to current aim"), Player->GetActorForwardVector().Equals(FVector::ForwardVector, .001f));
    Player->QuarterViewData = NewObject<UPGQuarterViewData>(Player);
    Player->QuarterViewData->bDodgeFollowsMovement = false;
    Player->MoveInputDirection = -FVector::ForwardVector;
    Player->FaceDodgeDirection();
    TestTrue(TEXT("Data can retain cursor-only dodge"), Player->GetActorForwardVector().Equals(FVector::ForwardVector, .001f));
    TestTrue(TEXT("Cursor-only option also controls the warp direction"), Player->GetDodgeDirection().Equals(FVector::ForwardVector, .001f));
    World->DestroyWorld(false);
    return true;
}
#endif

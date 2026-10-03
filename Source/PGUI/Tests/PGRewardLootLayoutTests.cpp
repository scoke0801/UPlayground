#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGUI/Widget/Window/RewardSelect/PGRewardPresentation.h"
#include "PGUI/Widget/Billboard/PGLootLabelLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRewardContextTest,"PG.UI.RewardBuildContext",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGRewardContextTest::RunTest(const FString& Parameters)
{
    FPGRewardStatDataRow Reward;
    Reward.BuildFamily=TEXT("Bleed");
    auto Empty=[](EPGCombatPerk){return 0;};
    auto EquippedBleed=[](EPGCombatPerk Perk){return Perk==EPGCombatPerk::Bleed ? 12 : 0;};
    TestTrue(TEXT("Starter is distinguished from current build"),PGRewardPresentation::BuildContext(Reward,Empty).ToString().Contains(TEXT("새 빌드")));
    TestTrue(TEXT("Equipped effects count toward synergy"),PGRewardPresentation::BuildContext(Reward,EquippedBleed).ToString().Contains(TEXT("현재 빌드")));
    Reward.RequiredPerk=EPGCombatPerk::Bleed; Reward.bKeystone=true;
    TestTrue(TEXT("Missing prerequisite takes priority over keystone badge"),PGRewardPresentation::BuildContext(Reward,Empty).ToString().Contains(TEXT("선행 강화 필요")));
    TestTrue(TEXT("Eligible keystone identified"),PGRewardPresentation::BuildContext(Reward,EquippedBleed).ToString().Contains(TEXT("핵심 강화")));
    Reward.RequiredPerks={EPGCombatPerk::BleedBurst};
    TestTrue(TEXT("All prerequisites checked"),PGRewardPresentation::BuildContext(Reward,EquippedBleed).ToString().Contains(TEXT("선행 강화 필요")));
    Reward=FPGRewardStatDataRow();
    TestTrue(TEXT("Shared reward is not presented as a new family"),PGRewardPresentation::BuildContext(Reward,Empty).ToString().Contains(TEXT("모든 빌드")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGLootLayoutTest,"PG.UI.LootLabelLayout",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGLootLayoutTest::RunTest(const FString& Parameters)
{
    for (const FVector2D View : {FVector2D(1280,720),FVector2D(1920,1080),FVector2D(2560,1080)})
    {
        const FSlateRect Bounds(310,170,View.X-24,View.Y-190);
        for (const FVector2D Anchor : {View*.5,FVector2D(0,0),View})
        {
            TArray<FSlateRect> Occupied;
            for (int32 I=0;I<100;++I)
            {
                FVector2D Point;
                if (!PGLootLabelLayout::Place(Anchor,FVector2D(260,48),Bounds,Occupied,Point)) continue;
                const FSlateRect Rect(Point.X,Point.Y,Point.X+260,Point.Y+48);
                TestTrue(TEXT("Packed label stays in safe combat area"),Rect.Left>=Bounds.Left && Rect.Top>=Bounds.Top && Rect.Right<=Bounds.Right && Rect.Bottom<=Bounds.Bottom);
                for (const auto& Other : Occupied)
                    TestFalse(TEXT("Dense drops never overlap"),Rect.Left<Other.Right && Rect.Right>Other.Left && Rect.Top<Other.Bottom && Rect.Bottom>Other.Top);
                Occupied.Add(Rect);
            }
            TestTrue(TEXT("Overflow is bounded without discarding highest priority label"),Occupied.Num()>0 && Occupied.Num()<100);
        }
    }
    FVector2D Point;
    TestFalse(TEXT("Impossible viewport hides instead of clipping labels"),PGLootLabelLayout::Place(FVector2D::ZeroVector,FVector2D(260,48),FSlateRect(0,0,100,20),{},Point));
    return true;
}
#endif

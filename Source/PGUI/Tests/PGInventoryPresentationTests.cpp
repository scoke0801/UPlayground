#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGUI/Widget/Window/PGInventoryPresentation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGInventoryComparisonTest,"PG.UI.InventoryComparison",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGInventoryComparisonTest::RunTest(const FString& Parameters)
{
    FPGItemInstance Current, Selected;
    Current.Options={{EPGStatType::Attack,100},{EPGStatType::Defense,35},{EPGStatType::CriticalRate,250}};
    Selected.Options={{EPGStatType::Attack,125},{EPGStatType::CriticalRate,375},{EPGStatType::Health,80}};
    const auto Rows=PGInventoryPresentation::Compare(Selected,&Current);
    TestEqual(TEXT("Union includes lost defense and new health"),Rows.Num(),4);
    const auto* Defense=Rows.FindByPredicate([](const auto& Row){return Row.Stat==EPGStatType::Defense;});
    TestTrue(TEXT("Removed defense remains visible as a loss"),Defense && Defense->Selected==0 && Defense->Delta()==-35);
    const auto* Crit=Rows.FindByPredicate([](const auto& Row){return Row.Stat==EPGStatType::CriticalRate;});
    TestTrue(TEXT("Crit delta is retained in authoritative basis points"),Crit && Crit->Delta()==125);
    TestEqual(TEXT("Crit chance displayed as percent"),PGInventoryPresentation::StatValue(EPGStatType::CriticalRate,375).ToString(),FString(TEXT("3.75%")));
    TestEqual(TEXT("Crit delta uses percentage points"),PGInventoryPresentation::StatValue(EPGStatType::CriticalRate,125,true).ToString(),FString(TEXT("+1.25%p")));
    TestEqual(TEXT("Negative crit delta keeps sign"),PGInventoryPresentation::StatValue(EPGStatType::CriticalRate,-125,true).ToString(),FString(TEXT("-1.25%p")));
    const auto EmptySlot=PGInventoryPresentation::Compare(Selected,nullptr);
    for (const auto& Row : EmptySlot) TestEqual(TEXT("Empty slot compares against zero"),Row.Current,0);
    const auto SameItem=PGInventoryPresentation::Compare(Selected,&Selected);
    for (const auto& Row : SameItem) TestEqual(TEXT("Selected equipped item has no gain"),Row.Delta(),0);
    for (int32 I=1;I<Rows.Num();++I) TestTrue(TEXT("Stable enum ordering"),uint8(Rows[I-1].Stat)<uint8(Rows[I].Stat));
    return true;
}
#endif

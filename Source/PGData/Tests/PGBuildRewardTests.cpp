#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGData/DataTable/Reward/PGRewardSelection.h"
#include "PGData/DataTable/Reward/PGRewardText.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGBuildRewardTest, "PG.Content.BuildRewardRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGBuildRewardTest::RunTest(const FString& Parameters)
{
    FPGRewardStatDataRow Reward;
    Reward.RequiredPerk = EPGCombatPerk::Shockwave;
    Reward.RequiredAnyPerks = {EPGCombatPerk::ShockEcho, EPGCombatPerk::ShockRadius};
    TMap<EPGCombatPerk,int32> Perks;
    auto Get = [&](EPGCombatPerk Perk) { return Perks.FindRef(Perk); };
    TestFalse(TEXT("No root cannot qualify"),Reward.MeetsRequirements(Get));
    Perks.Add(EPGCombatPerk::Shockwave,40);
    TestFalse(TEXT("Root alone cannot qualify"),Reward.MeetsRequirements(Get));
    Perks.Add(EPGCombatPerk::ShockEcho,65);
    TestTrue(TEXT("Root and one branch qualify"),Reward.MeetsRequirements(Get));
    Reward.RequiredPerks.Add(EPGCombatPerk::BleedBurst);
    TestFalse(TEXT("Additional AND requirement respected"),Reward.MeetsRequirements(Get));
    Perks.Add(EPGCombatPerk::BleedBurst,100);
    TestTrue(TEXT("All AND and OR requirements met"),Reward.MeetsRequirements(Get));
    Reward.RequiredAnyPerks.Reset(); Reward.RequiredPerks.Reset();
    TestTrue(TEXT("Legacy single requirement remains supported"),Reward.MeetsRequirements(Get));
    TArray<FPGStageReward> Pool;
    for (int32 Id=1;Id<=6;++Id) { FPGStageReward Entry; Entry.RewardId=Id; Entry.Weight=1; Pool.Add(Entry); }
    auto IsCore=[](int32 Id) { return Id>=5; };
    for (int32 Seed=1; Seed<=32; ++Seed)
    {
        FRandomStream A(Seed), B(Seed), Early(Seed);
        const auto First = PGRewardSelection::Draw(Pool,true,IsCore,A);
        const auto Again = PGRewardSelection::Draw(Pool,true,IsCore,B);
        const auto Before = PGRewardSelection::Draw(Pool,false,IsCore,Early);
        TestEqual(TEXT("Reservation preserves three cards"),First.Num(),3);
        TestTrue(TEXT("One reserved core"),IsCore(First[0].RewardId));
        TestFalse(TEXT("Second card is ordinary"),IsCore(First[1].RewardId));
        for (int32 I=0;I<3;++I)
        {
            TestEqual(TEXT("Seed reproduces complete offer"),First[I].RewardId,Again[I].RewardId);
            TestFalse(TEXT("Early stage excludes cores"),IsCore(Before[I].RewardId));
        }
        TestTrue(TEXT("No duplicate cards"),First[1].RewardId!=First[2].RewardId);
    }
    Pool.RemoveAll([&](const FPGStageReward& Entry) { return IsCore(Entry.RewardId); });
    FRandomStream EmptyCore(1);
    TestEqual(TEXT("No eligible core falls back to ordinary cards"),PGRewardSelection::Draw(Pool,true,IsCore,EmptyCore).Num(),3);
    UPGCombatTuningData* Tuning = NewObject<UPGCombatTuningData>();
    Tuning->BleedRefundFraction=.42f;
    TestTrue(TEXT("Card text reads tuning instead of serialized defaults"),PGRewardText::Effect(EPGCombatPerk::BleedRecast,1,*Tuning).Contains(TEXT("42%")));
    return true;
}
#endif

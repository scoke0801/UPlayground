#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/DataAsset/Progression/PGItemGeneration.h"
#include "PGData/DataAsset/Progression/PGLootRules.h"
#include <limits>

namespace
{
    FPGItemOptionDefinition MakeOption(const TCHAR* Id, EPGStatType Stat, float Weight = 1.f)
    {
        FPGItemOptionDefinition O;
        O.Id = Id; O.DisplayName = FText::FromString(TEXT("검사 옵션")); O.Stat = Stat; O.Weight = Weight;
        O.AllowedSlots = {EPGEquipmentSlot::Weapon};
        O.MinAtLowPower = 10; O.MaxAtLowPower = 20; O.MinAtHighPower = 30; O.MaxAtHighPower = 40;
        O.Cap = 100;
        return O;
    }
    FPGItemDataRow MakeDefinition()
    {
        FPGItemDataRow D; D.Id = 1; D.bUseOptionPool = true; D.MinPower = 10; D.MaxPower = 20;
        D.Rarity = EPGItemRarity::Rare;
        D.BaseStatRanges = {MakeOption(TEXT("Base"), EPGStatType::Attack)};
        D.OptionPool = {MakeOption(TEXT("Attack"), EPGStatType::Attack), MakeOption(TEXT("Health"), EPGStatType::Health),
            MakeOption(TEXT("Crit"), EPGStatType::CriticalRate), MakeOption(TEXT("Damage"), EPGStatType::CriticalDamage)};
        return D;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGItemGenerationTest, "PG.Progression.EquipmentOptionGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGItemGenerationTest::RunTest(const FString&)
{
    auto Def = MakeDefinition(); const auto Rules = PGEquipmentRules::DefaultRarities(); FString Error;
    int32 Min, Max;
    PGItemGeneration::Range(Def, Def.BaseStatRanges[0], 15, Min, Max);
    TestEqual(TEXT("Midpoint minimum"), Min, 20); TestEqual(TEXT("Midpoint maximum"), Max, 30);
    auto Reordered = Def;
    Reordered.OptionPool.Swap(0, 3); Reordered.OptionPool.Swap(1, 2);
    for (int32 Seed = 0; Seed < 256; ++Seed)
    {
        FRandomStream A(Seed), B(Seed); FPGItemInstance First, Second;
        if (!TestTrue(TEXT("Generate valid rare"), PGItemGeneration::Generate(Def, Rules[2], A, First, Error))) return false;
        TestTrue(TEXT("Generate reordered rare"), PGItemGeneration::Generate(Reordered, Rules[2], B, Second, Error));
        TestEqual(TEXT("Seed reproduces power"), First.Power, Second.Power);
        TestTrue(TEXT("Authoring order cannot change resolved values"), First.Options.OrderIndependentCompareEqual(Second.Options));
        TestEqual(TEXT("Stable random consumption"), A.GetCurrentSeed(), B.GetCurrentSeed());
        TestEqual(TEXT("Base plus two affixes"), First.OptionRolls.Num(), 3);
        TestTrue(TEXT("Power bounds"), First.Power >= 10 && First.Power <= 20);
        TestTrue(TEXT("Instance validates"), PGEquipmentRules::ValidateInstance(First));
        TestNotEqual(TEXT("Identity remains independent of seed"), First.Guid, Second.Guid);
        for (const auto& Roll : First.OptionRolls) TestTrue(TEXT("Normal range"), Roll.Value >= Roll.Min && Roll.Value <= Roll.Max);
    }
    FRandomStream Random(721); FPGItemInstance Item;
    Def.Rarity = EPGItemRarity::Epic; Def.DropQuality = EPGItemQuality::Ancestral;
    Def.OptionPool[0].ExclusiveGroup = TEXT("AttackChoice"); Def.OptionPool[1].ExclusiveGroup = TEXT("AttackChoice");
    Def.OptionPool[1].bAllowGreater = false;
    Def.GreaterCountWeights = {1.f, 2.f, 3.f};
    for (int32 Sample = 0; Sample < 256; ++Sample)
    {
        if (!TestTrue(TEXT("Ancestral with exclusions"), PGItemGeneration::Generate(Def, Rules[3], Random, Item, Error))) return false;
        TestTrue(TEXT("At least one greater"), Item.GreaterOptions.Num() >= 1);
        TSet<EPGStatType> Stats;
        for (const auto& Roll : Item.OptionRolls)
        {
            if (Roll.Source != EPGItemOptionSource::Affix) { TestFalse(TEXT("Base never greater"), Roll.bGreater); continue; }
            Stats.Add(Roll.Stat);
            if (Roll.bGreater) TestEqual(TEXT("Greater uses maximum times multiplier"), Roll.Value, FMath::RoundToInt(Roll.Max * 1.25f));
        }
        TestFalse(TEXT("Exclusive affixes never coexist"), Stats.Contains(EPGStatType::Attack) && Stats.Contains(EPGStatType::Health));
    }
    auto Broken = Def; Broken.OptionPool[2].ExclusiveGroup = TEXT("AttackChoice");
    TestFalse(TEXT("Insufficient nonexclusive candidates"), PGItemGeneration::Generate(Broken, Rules[3], Random, Item, Error));
    TestFalse(TEXT("Failure clears stale identity"), Item.Guid.IsValid());
    Broken = Def; for (auto& O : Broken.OptionPool) O.bAllowGreater = false;
    TestFalse(TEXT("No eligible greater blocked"), PGItemGeneration::Validate(Broken, Rules[3], Error));
    Broken = Def; Broken.OptionPool[0].Stat = Broken.OptionPool[1].Stat;
    TestFalse(TEXT("Duplicate stat blocked"), PGItemGeneration::Validate(Broken, Rules[3], Error));
    Broken = Def; Broken.OptionPool[0].MinAtLowPower = -1;
    TestFalse(TEXT("Negative range blocked"), PGItemGeneration::Validate(Broken, Rules[3], Error));
    Broken = Def; Broken.OptionPool[0].Weight = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite weight blocked"), PGItemGeneration::Validate(Broken, Rules[3], Error));
    Def = MakeDefinition(); Def.Rarity = EPGItemRarity::Unique;
    TestTrue(TEXT("Unique rolls four specified options"), PGItemGeneration::Generate(Def, Rules[5], Random, Item, Error));
    TestEqual(TEXT("Unique base plus four specified affixes"), Item.OptionRolls.Num(), 5);
    Item.Options.FindOrAdd(EPGStatType::Attack)++;
    TestFalse(TEXT("Inconsistent aggregate rejected"), PGEquipmentRules::ValidateInstance(Item));
    Def.OptionPool.Pop();
    TestFalse(TEXT("Unique missing specified option blocked"), PGItemGeneration::Validate(Def, Rules[5], Error));
    Def = MakeDefinition(); Def.Rarity = EPGItemRarity::Magic; Def.OptionPool.SetNum(2); Def.OptionPool[1].Weight = 3.f;
    int32 HealthCount = 0;
    for (int32 Sample = 0; Sample < 4096; ++Sample)
    {
        if (!PGItemGeneration::Generate(Def, Rules[1], Random, Item, Error)) return false;
        HealthCount += Item.OptionRolls.Last().Stat == EPGStatType::Health;
    }
    TestTrue(TEXT("Weighted sample stays near 75 percent"), HealthCount > 2800 && HealthCount < 3300);
    Def.Rarity = EPGItemRarity::Common;
    TestTrue(TEXT("Common has no affixes"), PGItemGeneration::Generate(Def, Rules[0], Random, Item, Error));
    TestEqual(TEXT("Common only base"), Item.OptionRolls.Num(), 1);
    Def = MakeDefinition(); Def.Rarity = EPGItemRarity::Epic; Def.DropQuality = EPGItemQuality::Ancestral;
    for (auto& O : Def.OptionPool) O.Cap = 40;
    Def.GreaterCountWeights = {1.f, 0.f, 100.f};
    TestTrue(TEXT("Capped greater generation"), PGItemGeneration::Generate(Def, Rules[3], Random, Item, Error));
    for (const auto& Roll : Item.OptionRolls) if (Roll.bGreater) TestTrue(TEXT("Individual greater cap"), Roll.Value <= 40);
    Def = MakeDefinition(); Def.Rarity = EPGItemRarity::Magic;
    Def.OptionPool[0].bEnabled = false; Def.OptionPool[1].Weight = 0.f;
    Def.OptionPool[2].AllowedSlots = {EPGEquipmentSlot::Feet};
    TestTrue(TEXT("Disabled zero-weight wrong-slot options excluded"), PGItemGeneration::Generate(Def, Rules[1], Random, Item, Error));
    TestEqual(TEXT("Only eligible option picked"), Item.OptionRolls.Last().Stat, EPGStatType::CriticalDamage);
    Def.Slot = EPGEquipmentSlot::RingRight;
    for (auto& O : Def.BaseStatRanges) O.AllowedSlots = {EPGEquipmentSlot::RingLeft};
    for (auto& O : Def.OptionPool) O.AllowedSlots = {EPGEquipmentSlot::RingLeft};
    TestTrue(TEXT("Both rings share authored pool"), PGItemGeneration::Generate(Def, Rules[1], Random, Item, Error));
    Def.Slot = EPGEquipmentSlot::Head;
    TestFalse(TEXT("Wrong base slot blocked"), PGItemGeneration::Validate(Def, Rules[1], Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGItemGenerationFallbackTest, "PG.Progression.EquipmentGenerationFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGItemGenerationFallbackTest::RunTest(const FString&)
{
    auto* Catalog = NewObject<UPGProgressionData>();
    auto Def = MakeDefinition(); Def.OptionPool.Reset();
    auto Good = MakeDefinition(); Good.Id = 2;
    Catalog->Items = {Def, Good};
    FPGDropPool Main; Main.Id = TEXT("Main"); Main.bGuaranteed = true; Main.Entries = {{1, 1.f}};
    FPGDropPool Fallback; Fallback.Id = TEXT("Fallback"); Fallback.bGuaranteed = true; Fallback.Entries = {{2, 1.f}};
    Catalog->DropPools = {Main, Fallback}; Catalog->InvalidEquipmentFallbackPool = Fallback.Id;
    FString Error;
    TestFalse(TEXT("Release validation rejects broken primary"), PGLootRules::ValidatePools(*Catalog, Error));
    FRandomStream Random(71); FPGItemInstance Item;
    AddExpectedError(TEXT("Equipment generation rejected:"), EAutomationExpectedErrorFlags::Contains, 2);
    TestTrue(TEXT("Runtime bad definition uses validated fallback"), PGLootRules::Roll(*Catalog, Main.Id, Random, Item));
    TestEqual(TEXT("Only valid replacement paid"), Item.DefinitionId, 2);
    Catalog->Items[1].OptionPool.Reset();
    TestFalse(TEXT("Invalid fallback fails without recursion"), PGLootRules::Roll(*Catalog, Main.Id, Random, Item));
    TestFalse(TEXT("No stale payout"), Item.Guid.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGAuthoredItemGenerationTest, "PG.Progression.AuthoredEquipmentOptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGAuthoredItemGenerationTest::RunTest(const FString&)
{
    const auto* Catalog=LoadObject<UPGProgressionData>(nullptr,TEXT("/Game/DataCenter/Progression/DA_PGProgression.DA_PGProgression"));
    if (!TestNotNull(TEXT("Actual progression catalog loads"),Catalog)) return false;
    FString Error;
    if (!TestTrue(TEXT("Actual catalog validates"),Catalog->ValidateEquipmentGeneration(Error))) { AddError(Error); return false; }
    for (int32 Id=15001;Id<=15006;++Id)
    {
        const auto* Def=Catalog->FindItem(Id);
        if (!TestTrue(TEXT("Authored definition uses U3"),Def && Def->bUseOptionPool)) return false;
        const auto* Rule=Catalog->RarityRules.FindByPredicate([&](const auto& R){ return R.Rarity==Def->Rarity; });
        if (!TestNotNull(TEXT("Authored rarity rule"),Rule)) return false;
        int32 AttackCount=0, HealthCount=0;
        TSet<EPGStatType> Seen;
        for (int32 Seed=0;Seed<4096;++Seed)
        {
            FRandomStream A(Seed), B(Seed); FPGItemInstance First, Second;
            if (!PGItemGeneration::Generate(*Def,*Rule,A,First,Error) || !PGItemGeneration::Generate(*Def,*Rule,B,Second,Error))
            { AddError(FString::Printf(TEXT("Item %d seed %d: %s"),Id,Seed,*Error)); return false; }
            TestTrue(TEXT("Actual catalog is seed reproducible"),First.Power==Second.Power && First.Options.OrderIndependentCompareEqual(Second.Options));
            TestTrue(TEXT("Authored power bounds"),First.Power>=Def->MinPower && First.Power<=Def->MaxPower);
            TestTrue(TEXT("Actual output validates"),PGEquipmentRules::ValidateInstance(First));
            int32 Affixes=0;
            for (const auto& Roll : First.OptionRolls)
            {
                TestTrue(TEXT("Authored range bounds"),Roll.Value>=Roll.Min && Roll.Value<=Roll.Max);
                if (Roll.Source!=EPGItemOptionSource::Affix) continue;
                ++Affixes; Seen.Add(Roll.Stat);
                AttackCount+=Roll.Stat==EPGStatType::Attack;
                HealthCount+=Roll.Stat==EPGStatType::Health;
            }
            TestEqual(TEXT("Actual rarity option count"),Affixes,Rule->OptionCount);
        }
        if (Rule->OptionCount>0) TestEqual(TEXT("All authored affixes reachable"),Seen.Num(),Def->OptionPool.Num());
        if (Id==15002) TestTrue(TEXT("Weapon attack weight near 60 percent"),AttackCount>2200 && AttackCount<2700);
        if (Id==15003) TestTrue(TEXT("Necklace health weight near 50 percent"),HealthCount>1800 && HealthCount<2300);
    }
    return true;
}
#endif

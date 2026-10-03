#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/DataAsset/Progression/PGLootRules.h"
#include "PGShared/Shared/Structure/PGRunRandom.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGLootRulesTest, "PG.Progression.DropPoolsAndIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGLootRulesTest::RunTest(const FString& Parameters)
{
    auto* Catalog = NewObject<UPGProgressionData>();
    for (int32 Id = 1; Id <= 3; ++Id)
    {
        FPGItemDataRow Item; Item.Id = Id; Item.RollBonus = 10;
        Item.BaseOptions = {{EPGStatType::Attack, Id * 10}, {EPGStatType::Defense, Id}};
        Catalog->Items.Add(Item);
    }
    Catalog->DropChance = 0.f;
    FPGDropPool Pool; Pool.Id = TEXT("Elite"); Pool.DropChance = 0.f; Pool.bGuaranteed = true;
    Pool.Entries = {{1, 0.f}, {2, 1.f}, {3, 4.f}};
    Catalog->DropPools.Add(Pool);
    FString Error;
    TestTrue(TEXT("Valid authored pools"), PGLootRules::ValidatePools(*Catalog, Error));
    auto* Reordered = DuplicateObject<UPGProgressionData>(Catalog, GetTransientPackage());
    Reordered->DropPools[0].Entries = {{3, 4.f}, {1, 0.f}, {2, 1.f}};
    Reordered->Items[1].BaseOptions = {{EPGStatType::Defense, 2}, {EPGStatType::Attack, 20}};
    const FGuid RunId = FGuid::NewGuid();
    for (int32 Seed = 1; Seed <= 64; ++Seed)
    {
        const int32 LootSeed = PGRunRandom::LootSeed(Seed, 4, 2, 15104, 0);
        FRandomStream A(LootSeed), B(LootSeed), Global(Seed);
        for (int32 Retry = 0; Retry < 70; ++Retry) Global.FRand();
        FPGItemInstance First, Second;
        TestTrue(TEXT("Elite replaces zero global and pool chances with one item"), PGLootRules::Roll(*Catalog, Pool.Id, A, First));
        TestTrue(TEXT("Reordered data remains selectable"), PGLootRules::Roll(*Reordered, Pool.Id, B, Second));
        TestTrue(TEXT("Zero weight cannot be selected"), First.DefinitionId == 2 || First.DefinitionId == 3);
        TestEqual(TEXT("Definition independent of authoring order"), First.DefinitionId, Second.DefinitionId);
        TestTrue(TEXT("Options independent of map order"), First.Options.OrderIndependentCompareEqual(Second.Options));
        TestTrue(TEXT("Roll stays in authored range"), First.Options[EPGStatType::Attack] >= First.DefinitionId * 10 && First.Options[EPGStatType::Attack] <= First.DefinitionId * 10 + 10);
    }
    const auto Identity = PGRunRandom::LootGuid(RunId, 4, 2, 15104, 0);
    TestEqual(TEXT("Checkpoint replay preserves loot identity"), Identity, PGRunRandom::LootGuid(RunId, 4, 2, 15104, 0));
    TestNotEqual(TEXT("Next spawn is independent"), Identity, PGRunRandom::LootGuid(RunId, 4, 2, 15104, 1));
    TestNotEqual(TEXT("New run with same seed gets fresh loot identities"), Identity, PGRunRandom::LootGuid(FGuid::NewGuid(), 4, 2, 15104, 0));
    FRandomStream Random(5); FPGItemInstance Item;
    TestFalse(TEXT("Legacy zero chance respected"), PGLootRules::Roll(*Catalog, NAME_None, Random, Item));
    Catalog->DropChance = 1.f;
    TestTrue(TEXT("Empty pool ID retains legacy catalog"), PGLootRules::Roll(*Catalog, NAME_None, Random, Item));
    TestFalse(TEXT("Unknown explicit pool cannot fall back"), PGLootRules::Roll(*Catalog, TEXT("Missing"), Random, Item));
    TestFalse(TEXT("Rejected roll clears stale output"), Item.Guid.IsValid());
    Catalog->DropPools[0].bGuaranteed = false;
    TestFalse(TEXT("Ordinary pool chance respected"), PGLootRules::Roll(*Catalog, Pool.Id, Random, Item));
    Catalog->DropPools[0].Entries.Add({999, 1.f});
    TestFalse(TEXT("Unknown item reference blocked"), PGLootRules::ValidatePools(*Catalog, Error));
    Catalog->DropPools[0] = Pool; Catalog->DropPools[0].Entries.Add({2, 1.f});
    TestFalse(TEXT("Duplicate weighted entries blocked"), PGLootRules::ValidatePools(*Catalog, Error));
    Catalog->DropPools[0] = Pool; Catalog->DropPools[0].Entries = {{1, 0.f}};
    TestFalse(TEXT("Empty effective guaranteed pool blocked"), PGLootRules::ValidatePools(*Catalog, Error));
    Catalog->DropPools[0] = Pool; Catalog->DropPools[0].DropChance = 2.f;
    TestFalse(TEXT("Invalid chance blocked"), PGLootRules::ValidatePools(*Catalog, Error));
    return true;
}
#endif

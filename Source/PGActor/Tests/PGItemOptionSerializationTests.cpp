#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSave.h"
#include "PGData/DataAsset/Progression/PGItemGeneration.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGItemOptionSerializationTest, "PG.Progression.EquipmentOptionSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGItemOptionSerializationTest::RunTest(const FString&)
{
    FPGItemDataRow Def; Def.Id = 711; Def.bUseOptionPool = true; Def.Rarity = EPGItemRarity::Epic;
    Def.DropQuality = EPGItemQuality::Ancestral;
    FPGItemOptionDefinition Base; Base.Id = TEXT("Base"); Base.Stat = EPGStatType::Attack;
    Base.DisplayName = FText::FromString(TEXT("공격력")); Base.AllowedSlots = {EPGEquipmentSlot::Weapon};
    Base.MinAtLowPower = Base.MinAtHighPower = 10; Base.MaxAtLowPower = Base.MaxAtHighPower = 20;
    Def.BaseStatRanges = {Base};
    for (auto Stat : {EPGStatType::Attack, EPGStatType::Health, EPGStatType::CriticalRate})
    {
        auto Affix = Base; Affix.Stat = Stat; Affix.Id = FName(*FString::Printf(TEXT("Affix%d"), uint8(Stat)));
        Def.OptionPool.Add(Affix);
    }
    auto* Save = NewObject<UPGProfileSave>();
    FRandomStream Random(731); FPGItemInstance Item; FString Error;
    if (!TestTrue(TEXT("Generate for serialization"), PGItemGeneration::Generate(Def, PGEquipmentRules::DefaultRarities()[3], Random, Item, Error))) return false;
    Item.bFavorite = true;
    Save->Items.Add(Item); Save->BossReward = Item;
    TArray<uint8> Bytes;
    TestTrue(TEXT("Save generated option snapshots"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
    const auto* Restored = Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Restore option snapshots"), Restored) || Restored->Items.Num() != 1) return false;
    const auto& Loaded = Restored->Items[0];
    TestTrue(TEXT("Favorite survives serialization"), Loaded.bFavorite && !Loaded.bJunk);
    TestTrue(TEXT("Resolved values preserved"), Loaded.Options.OrderIndependentCompareEqual(Item.Options));
    TestTrue(TEXT("Loaded inventory validates without catalog reroll"), PGEquipmentRules::ValidateInstance(Loaded));
    TestTrue(TEXT("Pending boss reward validates"), PGEquipmentRules::ValidateInstance(Restored->BossReward));
    TestEqual(TEXT("Snapshot count"), Loaded.OptionRolls.Num(), Item.OptionRolls.Num());
    if (Loaded.OptionRolls.Num() != Item.OptionRolls.Num()) return false;
    for (int32 I = 0; I < Item.OptionRolls.Num(); ++I)
    {
        const auto& A = Item.OptionRolls[I]; const auto& B = Loaded.OptionRolls[I];
        TestEqual(TEXT("Option identity"), A.Id, B.Id); TestEqual(TEXT("Stat"), A.Stat, B.Stat);
        TestEqual(TEXT("Source"), A.Source, B.Source); TestEqual(TEXT("Value"), A.Value, B.Value);
        TestEqual(TEXT("Minimum"), A.Min, B.Min); TestEqual(TEXT("Maximum"), A.Max, B.Max);
        TestEqual(TEXT("Greater flag"), A.bGreater, B.bGreater);
    }
    return true;
}
#endif

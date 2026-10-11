#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGSalvageTest, "PG.Progression.SalvageRulesAndProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGSalvageTest::RunTest(const FString&)
{
    FPGSalvageRules Rules;
    TestTrue(TEXT("Default economy valid"),Rules.IsValid());
    FPGItemInstance Item; Item.Guid=FGuid::NewGuid(); Item.DefinitionId=1; Item.ItemVersion=1;
    Item.Rarity=Item.OriginalRarity=EPGItemRarity::Epic; Item.Power=26;
    TMap<FName,int64> Yield;
    TestTrue(TEXT("Power tier yield"),Rules.GetYield(Item,Yield));
    TestEqual(TEXT("Epic with one power tier"),Yield.FindRef(TEXT("Shard")),int64(8));
    Item.Rarity=EPGItemRarity::Legendary;
    TestTrue(TEXT("Imprinted item salvage"),Rules.GetYield(Item,Yield));
    TestEqual(TEXT("Imprint does not create essence"),Yield.FindRef(TEXT("Essence")),int64(0));
    Item.OriginalRarity=EPGItemRarity::Legendary;
    TestTrue(TEXT("Natural legendary yield"),Rules.GetYield(Item,Yield));
    TestEqual(TEXT("Natural legendary essence"),Yield.FindRef(TEXT("Essence")),int64(1));
    Rules.Yields[0].PowerStep=0;
    TestFalse(TEXT("Zero division data rejected"),Rules.IsValid());
    Rules=FPGSalvageRules(); const FPGSalvageYield Duplicate=Rules.Yields[0]; Rules.Yields.Add(Duplicate);
    TestFalse(TEXT("Duplicate rarity rejected"),Rules.IsValid());
    Rules=FPGSalvageRules(); Rules.Yields[0].Materials.Add(TEXT("Missing"),1);
    TestFalse(TEXT("Unknown material rejected"),Rules.IsValid());

    auto* System=NewObject<UPGProfileSubsystem>(NewObject<UGameInstance>());
    System->Catalog=NewObject<UPGProgressionData>(System);
    System->Catalog->bRoguelikeRuns=true;
    FPGBuildDefinition Build; Build.Id=TEXT("Test"); System->Catalog->Builds.Add(Build);
    FPGItemDataRow Def; Def.Id=1; System->Catalog->Items.Add(Def);
    System->Profile=NewObject<UPGProfileSave>(System);
    System->Profile->Version=5; System->Profile->RunId=FGuid::NewGuid(); System->Profile->RunSeed=123;
    System->Profile->Growth=System->Catalog->LevelRules.InitialState();
    Item.bJunk=true; System->Profile->Items.Add(Item);
    System->TestSlotPrefix=TEXT("PGSalvageTest_")+FGuid::NewGuid().ToString()+TEXT("_");
    TestTrue(TEXT("v5 fixture writes"),UGameplayStatics::SaveGameToSlot(System->Profile,System->SlotName(0),0));
    System->LoadProfile();
    TestEqual(TEXT("Migrates v5 to v6"),System->Profile->Version,UPGProfileSave::CurrentVersion);
    TestTrue(TEXT("Migration starts without free materials"),System->Profile->Materials.IsEmpty());
    TestEqual(TEXT("Migration preserves item identity"),System->Profile->Items[0].Guid,Item.Guid);
    FString Reason;
    TestTrue(TEXT("Batch preview"),System->PreviewSalvage({Item.Guid},true,Yield,Reason));
    const auto Expected=Yield;
    TestFalse(TEXT("Duplicate IDs rejected"),System->PreviewSalvage({Item.Guid,Item.Guid},true,Yield,Reason));
    TestTrue(TEXT("Failure clears output"),Yield.IsEmpty());
    TestFalse(TEXT("Unknown ID rejects whole batch"),System->PreviewSalvage({Item.Guid,FGuid::NewGuid()},true,Yield,Reason));
    TestFalse(TEXT("Empty batch rejected"),System->PreviewSalvage({},true,Yield,Reason));
    System->Profile->Items[0].bFavorite=true;
    TestFalse(TEXT("Favorite protected even with junk flag"),System->PreviewSalvage({Item.Guid},true,Yield,Reason));
    System->Profile->Items[0].bFavorite=false;
    System->Profile->Equipment.Add(EPGEquipmentSlot::Weapon,Item.Guid);
    TestFalse(TEXT("Equipped protected"),System->PreviewSalvage({Item.Guid},false,Yield,Reason));
    System->Profile->Equipment.Reset(); System->Profile->Items[0].bJunk=false;
    TestFalse(TEXT("Unmarked item excluded from batch"),System->PreviewSalvage({Item.Guid},true,Yield,Reason));
    TestTrue(TEXT("Unmarked item allowed individually"),System->PreviewSalvage({Item.Guid},false,Yield,Reason));
    System->Profile->Materials.Add(TEXT("Shard"),MAX_int64);
    TestFalse(TEXT("Overflow rejected"),System->PreviewSalvage({Item.Guid},false,Yield,Reason));
    System->Profile->Materials.Reset();
    TestFalse(TEXT("No preparation world rejects mutation"),System->Salvage({Item.Guid},false,System->Profile->Revision,Expected));
    System->Profile->Materials.Add(TEXT("Shard"),42);
    TestTrue(TEXT("Materials snapshot persists"),System->Commit(DuplicateObject<UPGProfileSave>(System->Profile,System),false));
    System->LoadProfile();
    TestEqual(TEXT("Reload preserves count"),System->Profile->Materials.FindRef(TEXT("Shard")),int64(42));
    System->bInjectSaveFailure=true;
    TestFalse(TEXT("Failed reset preserves materials"),System->BeginNewRun(123));
    TestEqual(TEXT("No reset before persistence"),System->Profile->Materials.FindRef(TEXT("Shard")),int64(42));
    System->bInjectSaveFailure=false;
    TestTrue(TEXT("New run resets materials"),System->BeginNewRun(123));
    TestTrue(TEXT("New run empty materials"),System->Profile->Materials.IsEmpty());
    for (int32 Index=0;Index<2;++Index) UGameplayStatics::DeleteGameInSlot(System->SlotName(Index),0);
    return true;
}
#endif

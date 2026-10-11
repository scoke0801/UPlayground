#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGEquipmentMigrationTest, "PG.Progression.EquipmentV2Migration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGEquipmentMigrationTest::RunTest(const FString&)
{
    auto* System = NewObject<UPGProfileSubsystem>(NewObject<UGameInstance>());
    System->TestSlotPrefix = TEXT("PGEquipmentMigration_") + FGuid::NewGuid().ToString() + TEXT("_");
    System->Catalog = NewObject<UPGProgressionData>(System);
    FPGItemDataRow Def; Def.Id = 1; Def.Rarity = EPGItemRarity::Rare; Def.Power = 17;
    System->Catalog->Items.Add(Def);
    FPGBuildDefinition Build; Build.Id = TEXT("Test"); System->Catalog->Builds.Add(Build);
    auto* Legacy = NewObject<UPGProfileSave>(System);
    FPGItemInstance A; A.Guid = FGuid::NewGuid(); A.DefinitionId = 1; A.Options.Add(EPGStatType::Attack, 13);
    FPGItemInstance B = A; B.Guid = FGuid::NewGuid(); B.Options[EPGStatType::Attack] = 29;
    Legacy->Items = {A, B}; Legacy->Equipment.Add(EPGEquipmentSlot::Weapon, B.Guid);
    Legacy->Revision = 4; Legacy->RunSeed = 42; Legacy->RunId = FGuid::NewGuid();
    Legacy->ClaimedLoot.Add(A.Guid);
    TestTrue(TEXT("Isolated legacy fixture written"), UGameplayStatics::SaveGameToSlot(Legacy, System->SlotName(0), 0));
    TArray<uint8> Original, After;
    UGameplayStatics::LoadDataFromSlot(Original, System->SlotName(0), 0);
    System->bInjectSaveFailure = true;
    System->LoadProfile();
    TestTrue(TEXT("Failed migration blocks subsequent writes"), System->IsSaveBlocked());
    TestEqual(TEXT("Failed migration preserves old version"), System->Profile->Version, 1);
    UGameplayStatics::LoadDataFromSlot(After, System->SlotName(0), 0);
    TestTrue(TEXT("Failure leaves source bytes intact"), Original == After);
    System->bInjectSaveFailure = false;
    System->LoadProfile();
    TestFalse(TEXT("Valid migration unblocks"), System->IsSaveBlocked());
    TestEqual(TEXT("Version advances atomically"), System->Profile->Version, UPGProfileSave::CurrentVersion);
    TestEqual(TEXT("Migration adds exactly one revision"), System->Profile->Revision, int64(5));
    TestEqual(TEXT("First roll preserved"), System->Profile->Items[0].Options.FindRef(EPGStatType::Attack), 13);
    TestEqual(TEXT("Second roll preserved"), System->Profile->Items[1].Options.FindRef(EPGStatType::Attack), 29);
    TestEqual(TEXT("Equipment identity preserved"), System->Profile->Equipment.FindRef(EPGEquipmentSlot::Weapon), B.Guid);
    TestEqual(TEXT("Legacy rarity captured"), System->Profile->Items[0].Rarity, EPGItemRarity::Rare);
    TestTrue(TEXT("Claim ledger preserved"), System->Profile->ClaimedLoot.Contains(A.Guid));
    UGameplayStatics::LoadDataFromSlot(After, System->SlotName(0), 0);
    TestTrue(TEXT("Successful migration leaves source slot intact"), Original == After);
    System->Catalog->Items[0].Rarity = EPGItemRarity::Common;
    System->Catalog->Items[0].Power = 999;
    System->LoadProfile();
    TestEqual(TEXT("Reload never repeats migration"), System->Profile->Revision, int64(5));
    TestEqual(TEXT("Catalog rebalance cannot change saved rarity"), System->Profile->Items[0].Rarity, EPGItemRarity::Rare);
    TestEqual(TEXT("Catalog rebalance cannot change saved power"), System->Profile->Items[0].Power, 17);
    auto* V2 = DuplicateObject<UPGProfileSave>(System->Profile, System);
    V2->Version = 2; V2->Revision = 6;
    TestTrue(TEXT("V2 fixture saved"), UGameplayStatics::SaveGameToSlot(V2, System->SlotName(0), 0));
    System->bInjectSaveFailure = true;
    System->LoadProfile();
    TestTrue(TEXT("V2 failed migration blocks writes"), System->IsSaveBlocked());
    TestEqual(TEXT("V2 failure retains version"), System->Profile->Version, 2);
    System->bInjectSaveFailure = false;
    System->LoadProfile();
    TestEqual(TEXT("V2 advances to slot schema"), System->Profile->Version, UPGProfileSave::CurrentVersion);
    TestEqual(TEXT("V2 rolls preserved"), System->Profile->Items[1].Options.FindRef(EPGStatType::Attack), 29);
    auto* V3 = DuplicateObject<UPGProfileSave>(System->Profile,System);
    V3->Version=3; V3->Revision=System->Profile->Revision+1;
    UGameplayStatics::SaveGameToSlot(V3,System->SlotName(0),0);
    System->LoadProfile();
    TestEqual(TEXT("V3 advances to protected marks"),System->Profile->Version,UPGProfileSave::CurrentVersion);
    TestTrue(TEXT("Legacy items remain unmarked"),!System->Profile->Items[0].bFavorite && !System->Profile->Items[0].bJunk);
    TestTrue(TEXT("Favorite commits"),System->SetItemMark(A.Guid,true,false));
    System->LoadProfile();
    TestTrue(TEXT("Favorite survives disk reload"),System->Profile->Items[0].bFavorite);
    TestFalse(TEXT("Favorite cannot discard"),System->Discard(A.Guid));
    System->bInjectSaveFailure=true;
    TestFalse(TEXT("Mark failure rejected"),System->SetItemMark(A.Guid,false,true));
    TestTrue(TEXT("Mark failure preserves protection"),System->Profile->Items[0].bFavorite);
    System->bInjectSaveFailure=false;
    TestTrue(TEXT("Mark retry commits"),System->SetItemMark(A.Guid,false,true));
    TestFalse(TEXT("Equipped junk rejected"),System->SetItemMark(B.Guid,false,true));
    TestTrue(TEXT("Clear test marks"),System->SetItemMark(A.Guid,false,false));
    auto* TenSlots = DuplicateObject<UPGProfileSave>(System->Profile, System);
    TenSlots->Equipment.Reset(); TenSlots->Items.Reset();
    for (uint8 Slot = 0; Slot < uint8(EPGEquipmentSlot::Max); ++Slot)
    {
        FPGItemDataRow SlotDef; SlotDef.Id = 100 + Slot; SlotDef.Slot = EPGEquipmentSlot(Slot);
        System->Catalog->Items.Add(SlotDef);
        FPGItemInstance Instance = System->Profile->Items[0]; Instance.Guid = FGuid::NewGuid(); Instance.DefinitionId = SlotDef.Id;
        TenSlots->Items.Add(Instance); TenSlots->Equipment.Add(SlotDef.Slot, Instance.Guid);
    }
    TestTrue(TEXT("Ten slots validate"), System->Validate(TenSlots));
    TArray<uint8> SlotBytes;
    TestTrue(TEXT("Ten slots serialize"), UGameplayStatics::SaveGameToMemory(TenSlots, SlotBytes));
    auto* TenRestored = Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromMemory(SlotBytes));
    TestTrue(TEXT("Ten slots restore valid"), System->Validate(TenRestored));
    if (TenRestored) TestEqual(TEXT("All slot GUIDs retained"), TenRestored->Equipment.Num(), 10);
    TenSlots->Equipment[EPGEquipmentSlot::RingRight] = TenSlots->Equipment[EPGEquipmentSlot::RingLeft];
    TestFalse(TEXT("Duplicate GUID occupancy rejected"), System->Validate(TenSlots));
    auto* Future = DuplicateObject<UPGProfileSave>(System->Profile, System);
    Future->Items[0].ItemVersion = 99;
    UGameplayStatics::SaveGameToSlot(Future, System->SlotName(0), 0);
    System->LoadProfile();
    TestTrue(TEXT("Future item version blocks both slots"), System->IsSaveBlocked());
    Future->Items[0].ItemVersion = 1; Future->Items[0].DefinitionId = 999;
    UGameplayStatics::SaveGameToSlot(Future, System->SlotName(0), 0);
    System->LoadProfile();
    TestTrue(TEXT("Unknown definition preserves source read-only"), System->IsSaveBlocked());
    FString Error;
    TestTrue(TEXT("Seven authored grade rules valid"), PGEquipmentRules::ValidateRarities(System->Catalog->RarityRules, Error));
    TestFalse(TEXT("Mythic drops stay locked"), System->Catalog->RarityRules.Last().bDropEnabled);
    auto Invalid = System->Profile->Items[1]; Invalid.Quality = EPGItemQuality::Ancestral;
    TestFalse(TEXT("Rare cannot be ancestral"), PGEquipmentRules::ValidateInstance(Invalid));
    Invalid.Rarity = EPGItemRarity::Epic;
    TestFalse(TEXT("Ancestral requires greater option"), PGEquipmentRules::ValidateInstance(Invalid));
    Invalid.GreaterOptions.Add(EPGStatType::Attack);
    TestTrue(TEXT("Ancestral metadata serializable"), PGEquipmentRules::ValidateInstance(Invalid));
    UGameplayStatics::DeleteGameInSlot(System->SlotName(0), 0);
    UGameplayStatics::DeleteGameInSlot(System->SlotName(1), 0);
    // Original byte backups are retained as evidence under the unique test prefix.
    return true;
}
#endif

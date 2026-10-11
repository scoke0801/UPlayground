#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCraftingTest,"PG.Progression.TemperAndMasterwork",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCraftingTest::RunTest(const FString&)
{
    auto* System=NewObject<UPGProfileSubsystem>(NewObject<UGameInstance>());
    auto* Catalog=NewObject<UPGProgressionData>(System); System->Catalog=Catalog;
    FPGBuildDefinition Build; Build.Id=TEXT("Test"); Catalog->Builds.Add(Build);
    FPGItemDataRow Def; Def.Id=1; Def.Rarity=EPGItemRarity::Epic; Def.Slot=EPGEquipmentSlot::Weapon; Catalog->Items.Add(Def);
    FPGItemInstance Item; Item.Guid=FGuid::NewGuid(); Item.DefinitionId=1; Item.ItemVersion=1;
    Item.Rarity=Item.OriginalRarity=EPGItemRarity::Epic; Item.Power=31;
    FPGItemOptionRoll Base; Base.Id=TEXT("BaseAttack"); Base.Stat=EPGStatType::Attack; Base.Value=Base.Min=Base.Max=20;
    FPGItemOptionRoll Affix=Base; Affix.Id=TEXT("AffixAttack"); Affix.Source=EPGItemOptionSource::Affix; Affix.Value=Affix.Min=Affix.Max=10;
    Item.OptionRolls={Base,Affix}; Item.Options.Add(EPGStatType::Attack,30);
    FString Reason; FPGItemInstance Result; TMap<FName,int64> Cost;
    TestTrue(TEXT("Default crafting data"),PGCraftingRules::Validate(*Catalog,Reason));
    TestTrue(TEXT("Tempering preview"),PGCraftingRules::Preview(*Catalog,Item,TEXT("TemperAttack"),Result,Cost,Reason));
    TestEqual(TEXT("Power based deterministic value"),Result.Temper.Value,5);
    TestEqual(TEXT("Independent source totals"),Result.Options.FindRef(EPGStatType::Attack),35);
    TestEqual(TEXT("No input mutation"),Item.Options.FindRef(EPGStatType::Attack),30);
    const auto Tempered=Result;
    TestFalse(TEXT("Identical temper cannot consume materials"),PGCraftingRules::Preview(*Catalog,Tempered,TEXT("TemperAttack"),Result,Cost,Reason));
    TestTrue(TEXT("Replacement"),PGCraftingRules::Preview(*Catalog,Tempered,TEXT("TemperHealth"),Result,Cost,Reason));
    TestEqual(TEXT("Old temper removed"),Result.Options.FindRef(EPGStatType::Attack),30);
    TestEqual(TEXT("Replacement has one source"),Result.Options.FindRef(EPGStatType::Health),23);
    Catalog->TemperRecipes[0].bAllowAffixDuplicate=false;
    TestFalse(TEXT("Per recipe affix duplicate restriction"),PGCraftingRules::Preview(*Catalog,Item,TEXT("TemperAttack"),Result,Cost,Reason));
    Catalog->TemperRecipes[0].bAllowAffixDuplicate=true;
    TestFalse(TEXT("Wrong slot"),PGCraftingRules::Preview(*Catalog,Item,TEXT("TemperDefense"),Result,Cost,Reason));
    for (auto Grade : {EPGItemRarity::Common,EPGItemRarity::Magic,EPGItemRarity::Unique,EPGItemRarity::MythicUnique})
    {
        auto Restricted=Item; Restricted.Rarity=Restricted.OriginalRarity=Grade;
        TestFalse(TEXT("Temper rarity restrictions"),PGCraftingRules::Preview(*Catalog,Restricted,TEXT("TemperAttack"),Result,Cost,Reason));
    }
    for (auto Grade : {EPGItemRarity::Rare,EPGItemRarity::Epic,EPGItemRarity::Legendary,EPGItemRarity::Unique,EPGItemRarity::MythicUnique})
    {
        auto Upgraded=Item; Upgraded.Rarity=Upgraded.OriginalRarity=Grade;
        const int32 Limit=Grade==EPGItemRarity::Rare ? 3 : 5;
        for (int32 Level=1;Level<=Limit;++Level)
        {
            TestTrue(TEXT("Next step"),PGCraftingRules::Preview(*Catalog,Upgraded,NAME_None,Result,Cost,Reason));
            TestEqual(TEXT("Only base is amplified, no compounding"),Result.Options.FindRef(EPGStatType::Attack),30+Level);
            TestEqual(TEXT("Step stored"),Result.MasterworkLevel,Level); Upgraded=Result;
        }
        TestFalse(TEXT("Cap enforced"),PGCraftingRules::Preview(*Catalog,Upgraded,NAME_None,Result,Cost,Reason));
    }
    auto Greater=Item; Greater.Quality=EPGItemQuality::Ancestral; Greater.OptionRolls[1].bGreater=true;
    Greater.OptionRolls[1].Value=15; Greater.Options[EPGStatType::Attack]=35; Greater.GreaterOptions.Add(EPGStatType::Attack);
    TestTrue(TEXT("Ancestral upgrade"),PGCraftingRules::Preview(*Catalog,Greater,NAME_None,Result,Cost,Reason));
    TestEqual(TEXT("Greater affix unchanged"),Result.OptionRolls[1].Value,15);
    TestEqual(TEXT("Greater not amplified"),Result.Options[EPGStatType::Attack],36);
    auto Legacy=Item; Legacy.OptionRolls.Reset();
    TestFalse(TEXT("Cannot infer legacy base"),PGCraftingRules::Preview(*Catalog,Legacy,NAME_None,Result,Cost,Reason));
    auto Protected=Item; Protected.bFavorite=true;
    TestFalse(TEXT("Favorite protected"),PGCraftingRules::Preview(*Catalog,Protected,NAME_None,Result,Cost,Reason));
    Protected.bFavorite=false; Protected.bJunk=true;
    TestFalse(TEXT("Junk protected"),PGCraftingRules::Preview(*Catalog,Protected,NAME_None,Result,Cost,Reason));
    Catalog->MasterworkSteps[0].Cost.Add(TEXT("Shard"),-1);
    TestFalse(TEXT("Invalid cost"),PGCraftingRules::Validate(*Catalog,Reason));
    Catalog->MasterworkSteps=PGCraftingRules::DefaultMasterworks();
    auto Duplicate=Catalog->MasterworkSteps[0]; Catalog->MasterworkSteps.Add(Duplicate);
    TestFalse(TEXT("Duplicate step"),PGCraftingRules::Validate(*Catalog,Reason)); Catalog->MasterworkSteps.Pop();
    auto Corrupt=Tempered; ++Corrupt.Options[EPGStatType::Attack];
    TestFalse(TEXT("Corrupt total rejected"),PGEquipmentRules::ValidateInstance(Corrupt));
    System->Profile=NewObject<UPGProfileSave>(System); System->Profile->Version=7;
    System->Profile->Growth=Catalog->LevelRules.InitialState(); System->Profile->Items.Add(Item);
    System->TestSlotPrefix=TEXT("PGCraftingTest_")+FGuid::NewGuid().ToString()+TEXT("_");
    TestTrue(TEXT("v7 fixture saved"),UGameplayStatics::SaveGameToSlot(System->Profile,System->SlotName(0),0));
    System->LoadProfile(); TestEqual(TEXT("v7 migrates"),System->Profile->Version,UPGProfileSave::CurrentVersion);
    TestTrue(TEXT("Migration preserves original options"),System->Profile->Items[0].Options.OrderIndependentCompareEqual(Item.Options));
    TestFalse(TEXT("Insufficient materials"),System->PreviewCraft(Item.Guid,NAME_None,Result,Cost,Reason));
    System->Profile->Materials.Add(TEXT("Shard"),100); System->Profile->Materials.Add(TEXT("Essence"),10);
    System->Profile->Items[0]=Tempered;
    TestTrue(TEXT("Tempered masterwork preview"),System->PreviewCraft(Item.Guid,NAME_None,Result,Cost,Reason));
    TestEqual(TEXT("Temper not amplified"),Result.Options[EPGStatType::Attack],36);
    TestFalse(TEXT("Unsafe world cannot craft"),System->Craft(Item.Guid,NAME_None,System->Profile->Revision,Result,Cost));
    auto* Next=DuplicateObject<UPGProfileSave>(System->Profile,System);
    Next->Items[0]=Result; for (const auto& Pair : Cost) Next->Materials.FindOrAdd(Pair.Key)-=Pair.Value;
    System->bInjectSaveFailure=true;
    TestFalse(TEXT("Atomic failure"),System->Commit(Next,false));
    TestEqual(TEXT("Failure preserves level"),System->Profile->Items[0].MasterworkLevel,0);
    TestEqual(TEXT("Failure preserves cost"),System->Profile->Materials[TEXT("Shard")],int64(100));
    System->bInjectSaveFailure=false;
    TestTrue(TEXT("Retry"),System->Commit(Next,false)); System->LoadProfile();
    TestEqual(TEXT("Reload level"),System->Profile->Items[0].MasterworkLevel,1);
    TestEqual(TEXT("Reload temper"),System->Profile->Items[0].Temper.Value,5);
    TestEqual(TEXT("Reload total"),System->Profile->Items[0].Options[EPGStatType::Attack],36);
    TestEqual(TEXT("Reload materials"),System->Profile->Materials[TEXT("Shard")],int64(85));
    System->Profile->LegendaryRecipes.Add(TEXT("Bloodletting"));
    TestTrue(TEXT("Imprint crafted item"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason));
    TestEqual(TEXT("Imprint preserves masterwork"),Result.MasterworkLevel,1);
    TestEqual(TEXT("Imprint preserves temper"),Result.Temper.Value,5);
    TMap<FName,int64> BeforeYield,AfterYield;
    TestTrue(TEXT("Original salvage"),Catalog->SalvageRules.GetYield(Item,BeforeYield));
    TestTrue(TEXT("Crafted salvage"),Catalog->SalvageRules.GetYield(Result,AfterYield));
    TestTrue(TEXT("No craft or imprint salvage inflation"),BeforeYield.OrderIndependentCompareEqual(AfterYield));
    for (int32 Index=0;Index<2;++Index) UGameplayStatics::DeleteGameInSlot(System->SlotName(Index),0);
    return true;
}
#endif

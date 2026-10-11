#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataAsset/Progression/PGLootRules.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGLegendaryTest,"PG.Progression.LegendaryImprint",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGLegendaryTest::RunTest(const FString&)
{
    auto* System=NewObject<UPGProfileSubsystem>(NewObject<UGameInstance>());
    auto* Catalog=NewObject<UPGProgressionData>(System); System->Catalog=Catalog;
    Catalog->bRoguelikeRuns=true; FPGBuildDefinition Build; Build.Id=TEXT("Test"); Catalog->Builds.Add(Build);
    FPGItemDataRow Def; Def.Id=1; Def.Rarity=EPGItemRarity::Epic; Def.Slot=EPGEquipmentSlot::Weapon; Catalog->Items.Add(Def);
    FPGItemInstance Item; Item.Guid=FGuid::NewGuid(); Item.DefinitionId=1; Item.ItemVersion=1;
    Item.Rarity=Item.OriginalRarity=EPGItemRarity::Epic; Item.Options.Add(EPGStatType::Attack,17);
    System->Profile=NewObject<UPGProfileSave>(System); System->Profile->Version=6;
    System->Profile->Growth=Catalog->LevelRules.InitialState(); System->Profile->Items.Add(Item);
    System->Profile->RunId=FGuid::NewGuid(); System->Profile->RunSeed=123;
    System->TestSlotPrefix=TEXT("PGLegendaryTest_")+FGuid::NewGuid().ToString()+TEXT("_");
    TestTrue(TEXT("v6 fixture"),UGameplayStatics::SaveGameToSlot(System->Profile,System->SlotName(0),0));
    System->LoadProfile(); TestEqual(TEXT("v6 migration"),System->Profile->Version,UPGProfileSave::CurrentVersion);
    TestEqual(TEXT("Identity preserved"),System->Profile->Items[0].Guid,Item.Guid);
    FString Reason; FPGItemInstance Result; TMap<FName,int64> Cost;
    TestTrue(TEXT("Default effects valid"),PGLegendaryRules::Validate(*Catalog,Reason));
    TestFalse(TEXT("Locked recipe"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason));
    System->Profile->LegendaryRecipes.Add(TEXT("Bloodletting"));
    TestFalse(TEXT("Insufficient currency"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason));
    System->Profile->Materials.Add(TEXT("Shard"),100); System->Profile->Materials.Add(TEXT("Essence"),5);
    TestTrue(TEXT("Imprint preview"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason));
    TestEqual(TEXT("Promotes rarity"),Result.Rarity,EPGItemRarity::Legendary);
    TestEqual(TEXT("Preserves original rarity"),Result.OriginalRarity,EPGItemRarity::Epic);
    TestTrue(TEXT("Preserves rolls"),Result.Options.OrderIndependentCompareEqual(Item.Options));
    TestEqual(TEXT("No preview mutation"),System->Profile->Items[0].Rarity,EPGItemRarity::Epic);
    TestFalse(TEXT("No preparation world"),System->Imprint(Item.Guid,TEXT("Bloodletting"),System->Profile->Revision,15,Cost));
    System->Profile->Items[0].bFavorite=true;
    TestFalse(TEXT("Protected"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason));
    System->Profile->Items[0].bFavorite=false; Catalog->Items[0].Slot=EPGEquipmentSlot::Feet;
    TestFalse(TEXT("Wrong slot"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason));
    Catalog->Items[0].Slot=EPGEquipmentSlot::Weapon;
    for (auto Rarity : {EPGItemRarity::Rare,EPGItemRarity::Unique,EPGItemRarity::MythicUnique})
    { System->Profile->Items[0].Rarity=Rarity; TestFalse(TEXT("Restricted rarity"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason)); }
    System->Profile->Items[0]=Item;
    TestTrue(TEXT("Valid again"),System->PreviewImprint(Item.Guid,TEXT("Bloodletting"),Result,Cost,Reason));
    auto Second=Result; Second.Guid=FGuid::NewGuid(); Second.DefinitionId=2;
    auto Gloves=Def; Gloves.Id=2; Gloves.Slot=EPGEquipmentSlot::Hands; Catalog->Items.Add(Gloves);
    TArray<FPGItemInstance> Items={Second,Result}; TMap<EPGEquipmentSlot,FGuid> Equipment;
    Equipment.Add(EPGEquipmentSlot::Hands,Second.Guid); Equipment.Add(EPGEquipmentSlot::Weapon,Result.Guid);
    TestEqual(TEXT("Tie uses fixed slot order"),PGLegendaryRules::Active(*Catalog,Items,Equipment).FindRef(TEXT("Bloodletting")),Result.Guid);
    Items[0].LegendaryPercent=25;
    TestEqual(TEXT("Strongest only"),PGLegendaryRules::Perks(*Catalog,Items,Equipment).FindRef(EPGCombatPerk::Bleed),25);
    Equipment.Remove(EPGEquipmentSlot::Hands);
    TestEqual(TEXT("Unequip reactivates weaker"),PGLegendaryRules::Perks(*Catalog,Items,Equipment).FindRef(EPGCombatPerk::Bleed),15);
    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto MakeASC=[World]()
    {
        auto* Actor=World->SpawnActor<AActor>(); auto* ASC=NewObject<UPGAbilitySystemComponent>(Actor);
        Actor->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Actor,Actor); return ASC;
    };
    auto* Source=MakeASC(); auto* Target=MakeASC();
    Source->InitializeCombatStats({{EPGStatType::Attack,100},{EPGStatType::Health,100}});
    Target->InitializeCombatStats({{EPGStatType::Health,1000}});
    Source->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),50.f);
    Items[1].LegendaryEffect=TEXT("Siphon"); Items[1].LegendaryPercent=3;
    Source->SetCombatPerks(PGLegendaryRules::Perks(*Catalog,Items,Equipment));
    EPGDamageType DamageType; const float Damage=Target->ReceiveCombatHit(Source,DamageType);
    TestTrue(TEXT("Legendary damage reaches GAS"),Damage>0);
    TestEqual(TEXT("Saved legendary strength heals on actual hit"),Source->GetHealth(),50.f+Damage*.03f);
    Source->SetCombatPerks({}); const float Health=Source->GetHealth(); Target->ReceiveCombatHit(Source,DamageType);
    TestEqual(TEXT("Removing legendary removes healing"),Source->GetHealth(),Health);
    World->DestroyWorld(false);
    TestTrue(TEXT("Recipe snapshot"),System->Commit(DuplicateObject<UPGProfileSave>(System->Profile,System),false));
    System->LoadProfile(); TestTrue(TEXT("Recipe persists"),System->Profile->LegendaryRecipes.Contains(TEXT("Bloodletting")));
    System->bInjectSaveFailure=true; TestFalse(TEXT("Failed reset"),System->BeginNewRun(123));
    TestEqual(TEXT("Currency preserved on failure"),System->Profile->Materials.FindRef(TEXT("Shard")),int64(100));
    System->bInjectSaveFailure=false; TestTrue(TEXT("New run"),System->BeginNewRun(123));
    TestTrue(TEXT("Permanent recipe"),System->Profile->LegendaryRecipes.Contains(TEXT("Bloodletting")));
    TestTrue(TEXT("Run currency cleared"),System->Profile->Materials.IsEmpty());
    Catalog->LegendaryEffects[0].Cost.Add(TEXT("Shard"),-1);
    TestFalse(TEXT("Negative cost rejected"),PGLegendaryRules::Validate(*Catalog,Reason));
    for (int32 Index=0;Index<2;++Index) UGameplayStatics::DeleteGameInSlot(System->SlotName(Index),0);
    return true;
}
#endif

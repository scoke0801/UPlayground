#include "PGActor/Controllers/PGPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Rendering/PGCharacterAppearanceComponent.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataAsset/Progression/PGItemGeneration.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"

// Three independent processes exercise public transactions against an isolated profile.
void PGRunInventoryCycleProbe(APGPlayerController* Controller, const FString& Action)
{
    FString TestName;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),TestName) || !TestName.StartsWith(TEXT("UI_U8_"))) return;
    int32 Failures=0;
    auto Check=[&](bool bOK,const TCHAR* Label)
    { if (!bOK) { ++Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryU8 FAIL %s"),Label); } return bOK; };
    auto* System=UPGProfileSubsystem::Get(Controller);
    auto* Player=Cast<APGCharacterPlayer>(Controller->GetPawn());
    APGStageManager* Stage=nullptr;
    for (TActorIterator<APGStageManager> It(Controller->GetWorld());It;++It) { Stage=*It; break; }
    if (!Check(System && Player && Stage && System->GetProfile() && !System->IsSaveBlocked(),TEXT("ready"))) return;
    const auto* Catalog=System->GetCatalog();
    const FString ExpectedSlot=TEXT("PGTest_")+TestName.Left(40)+TEXT("_Expected");
    const FGuid Run=System->GetProfile()->RunId;
    auto Token=[&](const TCHAR* Name) { return FGuid::NewDeterministicGuid(Run.ToString()+Name); };
    auto Generate=[&](int32 Id,const TCHAR* Name)
    {
        FPGItemInstance Item; FString Error; FRandomStream Random(173001+Id);
        const auto* Def=Catalog->FindItem(Id);
        const auto* Rule=Def ? Catalog->RarityRules.FindByPredicate([&](const auto& R){return R.Rarity==Def->Rarity;}) : nullptr;
        if (Check(Def && Rule,TEXT("definition"))) Check(PGItemGeneration::Generate(*Def,*Rule,Random,Item,Error),TEXT("generate"));
        Catalog->InitializeLegendary(Item); Item.Guid=Token(Name); return Item;
    };
    auto Ground=[&](const FPGItemInstance& Item)
    {
        auto* Drop=Controller->GetWorld()->SpawnActor<APGLootDrop>(Player->GetActorLocation(),FRotator::ZeroRotator);
        if (Check(Drop!=nullptr,TEXT("ground spawn"))) Drop->InitializeItem(Item);
        return Drop;
    };
    auto SaveExpected=[&]()
    { Check(UGameplayStatics::SaveGameToSlot(DuplicateObject<UPGProfileSave>(System->GetProfile(),GetTransientPackage()),ExpectedSlot,0),TEXT("expected snapshot")); };
    auto Compare=[&]()
    {
        auto* Expected=Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(ExpectedSlot,0));
        if (!Check(Expected!=nullptr,TEXT("previous process snapshot"))) return;
        for (TFieldIterator<FProperty> It(UPGProfileSave::StaticClass());It;++It)
            if (It->HasAnyPropertyFlags(CPF_SaveGame)) Check(It->Identical_InContainer(Expected,System->GetProfile()),*It->GetName());
        auto* ASC=Player->GetPGAbilitySystemComponent();
        const float Attack=ASC->GetStatDetails(EPGStatType::Attack).Current;
        const float Health=ASC->GetHealth();
        Check(System->RestorePlayer(Player),TEXT("restore GAS"));
        Check(FMath::IsNearlyEqual(Attack,ASC->GetStatDetails(EPGStatType::Attack).Current) && Health==ASC->GetHealth(),TEXT("restore does not stack or heal"));
    };
    const auto Equipment=Generate(15007,TEXT("equipment"));
    const auto Material=Generate(15008,TEXT("salvaged"));
    const auto Unclaimed=Generate(15002,TEXT("unclaimed"));
    if (Action==TEXT("u8seed"))
    {
        const auto* Original=Player->AppearanceComponent->GetAppearance();
        if (!Original && !Catalog->PlayableCharacters.IsEmpty()) Original=Catalog->PlayableCharacters[0].LoadSynchronous();
        const UPGCharacterAppearance* Other=nullptr;
        for (const auto& Ref : Catalog->PlayableCharacters)
            if (const auto* Appearance=Ref.LoadSynchronous(); Appearance && Original && Appearance->Id!=Original->Id)
            { Other=Appearance; break; }
        if (Check(Original && Other,TEXT("two playable character definitions")))
        {
            const auto Items=System->GetProfile()->Items;
            Check(System->SelectCharacter(Original->Id),TEXT("bind original character"));
            const auto* BeforeSwitch=System->GetProfile(); System->bInjectSaveFailure=true;
            Check(!System->SelectCharacter(Other->Id),TEXT("failed live character switch")); System->bInjectSaveFailure=false;
            Check(System->GetProfile()==BeforeSwitch && Player->AppearanceComponent->GetAppearance()==Original,TEXT("failed switch keeps appearance and ownership"));
            Check(System->SelectCharacter(Other->Id),TEXT("switch character before combat"));
            Check(System->GetProfile()->CharacterId==Other->Id && Player->AppearanceComponent->GetAppearance()==Other,TEXT("saved character matches visible character"));
            Check(System->SelectCharacter(Original->Id),TEXT("restore original character"));
            Check(System->GetProfile()->RunId==Run && System->GetProfile()->Items.Num()==Items.Num(),TEXT("switch preserves unstarted dungeon and original gear"));
            for (const auto& Item : Items)
                Check(System->GetProfile()->Items.ContainsByPredicate([&Item](const auto& Owned){return Owned.Guid==Item.Guid;}),TEXT("original gear identity restored"));
        }
        Check(System->MarkRunAssisted(),TEXT("assisted"));
        auto* Drop=Ground(Equipment);
        const auto* Before=System->GetProfile();
        System->bInjectSaveFailure=true;
        Check(Drop && !Drop->TryPickup(Player),TEXT("failed pickup"));
        System->bInjectSaveFailure=false;
        Check(IsValid(Drop) && System->GetProfile()==Before,TEXT("failed pickup preserves ground and save"));
        Check(Drop && Drop->TryPickup(Player),TEXT("pickup retry"));
        Check(!IsValid(Drop) && !System->TryPickup(Equipment),TEXT("remove only after save and reject duplicate"));
        Before=System->GetProfile(); System->bInjectSaveFailure=true;
        Check(!System->MoveToStash(Equipment.Guid),TEXT("failed stash transaction")); System->bInjectSaveFailure=false;
        Check(System->GetProfile()==Before,TEXT("failed stash keeps bag"));
        Check(System->MoveToStash(Equipment.Guid),TEXT("stash in live preparation"));
        Check(!System->TryPickup(Equipment),TEXT("stashed identity cannot duplicate"));
        Check(System->WithdrawFromStash(Equipment.Guid),TEXT("withdraw in live preparation"));
        Check(System->Equip(Equipment.Guid),TEXT("equip"));
        TArray<FGuid> Targets;
        for (int32 Index=0;Index<10;++Index)
        {
            auto Item=Material; Item.Guid=Index ? Token(*FString::Printf(TEXT("material%d"),Index)) : Material.Guid;
            Check(System->TryPickup(Item),TEXT("material pickup")); Targets.Add(Item.Guid);
        }
        TMap<FName,int64> Yield; FString Reason;
        Check(System->PreviewSalvage(Targets,false,Yield,Reason),TEXT("salvage preview"));
        Check(System->Salvage(Targets,false,System->GetProfile()->Revision,Yield),TEXT("salvage unlock"));
        FPGItemInstance Result; TMap<FName,int64> Cost;
        Check(System->PreviewCraft(Equipment.Guid,TEXT("TemperHealth"),Result,Cost,Reason),TEXT("temper preview"));
        Check(System->Craft(Equipment.Guid,TEXT("TemperHealth"),System->GetProfile()->Revision,Result,Cost),TEXT("temper"));
        Check(System->PreviewCraft(Equipment.Guid,NAME_None,Result,Cost,Reason),TEXT("masterwork preview"));
        Check(System->Craft(Equipment.Guid,NAME_None,System->GetProfile()->Revision,Result,Cost),TEXT("masterwork"));
        Check(System->PreviewImprint(Equipment.Guid,TEXT("Bloodletting"),Result,Cost,Reason),TEXT("imprint preview"));
        Check(System->Imprint(Equipment.Guid,TEXT("Bloodletting"),System->GetProfile()->Revision,Result.LegendaryPercent,Cost),TEXT("imprint"));
        Check(System->GrantExperience(Run,Token(TEXT("kill")),650),TEXT("experience"));
        auto Draft=System->GetProfile()->Growth.Investments; Draft.FindOrAdd(EPGStatType::Attack)+=1;
        Check(System->ApplyInvestments(Run,System->GetProfile()->Growth.Investments,Draft),TEXT("investment"));
        Check(System->CommitReward(Token(TEXT("reward")),2,EPGStatType::Attack,1),TEXT("checkpoint"));
        Ground(Unclaimed);
        Check(!System->HasClaimedLoot(Unclaimed.Guid),TEXT("unclaimed ground not persisted as acquired"));
        SaveExpected();
    }
    else if (Action==TEXT("u8resume"))
    {
        Compare();
        Check(Stage->GetCurrentStageId()==2,TEXT("stage checkpoint restored"));
        const auto* Before=System->GetProfile();
        Check(!System->TryPickup(Equipment) && !System->TryPickup(Material),TEXT("owned and salvaged loot replay"));
        Check(System->GrantExperience(Run,Token(TEXT("kill")),650),TEXT("experience replay idempotent"));
        Check(!System->CommitReward(Token(TEXT("reward")),2,EPGStatType::Attack,1),TEXT("reward replay rejected"));
        Check(System->GetProfile()==Before,TEXT("replays do not commit"));
        // Ground actors are recreated by encounter seeds; arbitrary world transforms are not saved.
        auto* Drop=Ground(Unclaimed);
        Check(Drop && Drop->TryPickup(Player) && !System->TryPickup(Unclaimed),TEXT("unclaimed replay acquired once"));
        auto Boss=Generate(15010,TEXT("boss"));
        Check(System->QueueBossReward(Boss),TEXT("boss queued"));
        Before=System->GetProfile(); System->bInjectSaveFailure=true;
        Check(!System->EndRun(true,6),TEXT("failed victory")); System->bInjectSaveFailure=false;
        Check(System->GetProfile()==Before && System->GetPendingBossReward().Guid==Boss.Guid,TEXT("victory failure preserves pending"));
        Check(System->EndRun(true,6),TEXT("victory retry"));
        Before=System->GetProfile(); Check(System->EndRun(true,6) && System->GetProfile()==Before,TEXT("victory exactly once"));
        SaveExpected();
    }
    else if (Action==TEXT("u8result"))
    {
        Compare();
        Check(Stage->GetCurrentStageState()==EPGStageState::Finished && Stage->GetCurrentStageId()==6,TEXT("boss result restored"));
        const auto* Before=System->GetProfile();
        Check(System->EndRun(true,6) && System->GetProfile()==Before,TEXT("reloaded victory exactly once"));
        const auto Recipes=Before->LegendaryRecipes; const int32 Wins=Before->CompletedRuns;
        System->bInjectSaveFailure=true; Check(!System->BeginNewRun(173002),TEXT("failed new run")); System->bInjectSaveFailure=false;
        Check(System->GetProfile()==Before,TEXT("new run failure atomic"));
        Check(System->BeginNewRun(173002),TEXT("new run retry"));
        const auto* Fresh=System->GetProfile();
        Check(Fresh->RunId!=Run && Fresh->Checkpoint==1 && !Fresh->bRunEnded && !Fresh->BossReward.Guid.IsValid(),TEXT("new identity"));
        Check(Fresh->ClaimedLoot.IsEmpty() && Fresh->ExperienceClaims.IsEmpty() && Fresh->RewardBonuses.IsEmpty() && Fresh->CombatPerks.IsEmpty(),TEXT("dungeon rewards reset"));
        Check(Fresh->Materials.OrderIndependentCompareEqual(Before->Materials) &&
            FPGLevelState::StaticStruct()->CompareScriptStruct(&Fresh->Growth,&Before->Growth,0),TEXT("permanent growth and materials"));
        Check(Fresh->Items.Num()==Before->Items.Num() && Fresh->Equipment.OrderIndependentCompareEqual(Before->Equipment),TEXT("permanent gear"));
        for (int32 Index=0;Index<Before->Items.Num();++Index)
            Check(FPGItemInstance::StaticStruct()->CompareScriptStruct(&Fresh->Items[Index],&Before->Items[Index],0),TEXT("exact owned instance retained"));
        Check(Fresh->Stash.ContainsByPredicate([Before](const auto& Item){return Item.Guid==Before->BossReward.Guid;}),TEXT("boss reward retained in stash"));
        Check(Fresh->LegendaryRecipes.Num()==Recipes.Num() && Fresh->CompletedRuns==Wins,TEXT("permanent recipes and wins retained"));
        for (const FName Recipe : Recipes) Check(Fresh->LegendaryRecipes.Contains(Recipe),TEXT("recipe retained"));
        Check(!System->GrantExperience(Run,Token(TEXT("kill")),650),TEXT("old run experience rejected"));
    }
    else Check(false,TEXT("unknown phase"));
    UE_LOG(LogTemp,Display,TEXT("PGInventoryU8 phase=%s failures=%d revision=%lld"),*Action,Failures,System->GetProfile()->Revision);
    Controller->ConsoleCommand(TEXT("quit"));
}
#endif

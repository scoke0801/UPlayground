#include "PGActor/Controllers/PGPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGUI/Widget/Window/PGInventoryPresentation.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGUI/Widget/Window/PGUIInventory.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataAsset/Progression/PGItemGeneration.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Engine/Texture2D.h"
#include "AssetCompilingManager.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "PGData/DataAsset/Input/DataAsset_InputConfig.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
void PGRunInventoryCycleProbe(APGPlayerController* Controller, const FString& Action);
#endif

void APGPlayerController::PGInventoryProbe(FString Action)
{
#if !UE_BUILD_SHIPPING
    FString TestName;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),TestName) || !TestName.StartsWith(TEXT("UI_")))
    { UE_LOG(LogTemp,Warning,TEXT("PGInventoryProbe requires an isolated -PGTestProfile=UI_*")); return; }
    if (Action.StartsWith(TEXT("u8")))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Action](float)
        { PGRunInventoryCycleProbe(this,Action); return false; }),2.f);
        return;
    }
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->GetProfile() || !Profile->GetCatalog()) return;
    const auto* Catalog=Profile->GetCatalog();
    if (Action==TEXT("verifyCraft"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){PGInventoryProbe(TEXT("craft"));return false;}),2.f);
        return;
    }
    if (Action==TEXT("craft"))
    {
        int32 Failures=0;
        auto Check=[&](bool OK,const TCHAR* Name){if (!OK) {++Failures;UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe CRAFT FAIL %s"),Name);}};
        if (!InventoryWidget || !Catalog->FindItem(15008)) {UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe CRAFT missing fixture"));return;}
        auto Generate=[&](int32 Id)
        {
            FPGItemInstance Item; FString Error; FRandomStream Random(Id);
            const auto* Def=Catalog->FindItem(Id);
            const auto* Rule=Catalog->RarityRules.FindByPredicate([&](const auto& R){return R.Rarity==Def->Rarity;});
            Check(PGItemGeneration::Generate(*Def,*Rule,Random,Item,Error),TEXT("generate"));
            Catalog->InitializeLegendary(Item); Check(Profile->TryPickup(Item),TEXT("pickup")); return Item;
        };
        const auto Epic=Generate(15007); TArray<FGuid> Targets;
        for (int32 Index=0;Index<10;++Index) Targets.Add(Generate(15008).Guid);
        TMap<FName,int64> Yield; FString Reason;
        Check(Profile->PreviewSalvage(Targets,false,Yield,Reason),TEXT("funding preview"));
        Check(Profile->Salvage(Targets,false,Profile->GetProfile()->Revision,Yield),TEXT("funding"));
        if (Failures) { UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe CRAFT fixture failed")); return; }
        Check(Profile->Equip(Epic.Guid),TEXT("equip crafting target"));
        InventoryWidget->SelectItem(Epic.Guid);
        Check(InventoryWidget->PreviewCraft(TEXT("TemperAttack")),TEXT("temper preview"));
        InventoryWidget->HandleEscape(); Check(!InventoryWidget->HasPendingEdit(),TEXT("escape cancels"));
        Check(InventoryWidget->PreviewCraft(TEXT("TemperAttack")),TEXT("preview again"));
        const auto* Before=Profile->GetProfile(); FPGItemInstance Expected; TMap<FName,int64> Cost;
        Check(Profile->PreviewCraft(Epic.Guid,TEXT("TemperAttack"),Expected,Cost,Reason),TEXT("quote"));
        Check(!Profile->Craft(Epic.Guid,TEXT("TemperAttack"),Before->Revision-1,Expected,Cost),TEXT("stale revision"));
        auto Wrong=Expected; ++Wrong.Temper.Value;
        Check(!Profile->Craft(Epic.Guid,TEXT("TemperAttack"),Before->Revision,Wrong,Cost),TEXT("changed result"));
        auto WrongCost=Cost; WrongCost.Add(TEXT("Shard"),1);
        Check(!Profile->Craft(Epic.Guid,TEXT("TemperAttack"),Before->Revision,Expected,WrongCost),TEXT("changed cost"));
        Profile->bInjectSaveFailure=true; Check(!InventoryWidget->ConfirmCraft(),TEXT("save failure")); Profile->bInjectSaveFailure=false;
        Check(Profile->GetProfile()==Before && InventoryWidget->HasPendingEdit(),TEXT("failure retains snapshot and draft"));
        Check(InventoryWidget->ConfirmCraft(),TEXT("retry")); Check(!InventoryWidget->ConfirmCraft(),TEXT("double confirm"));
        Check(!Profile->Craft(Epic.Guid,TEXT("TemperAttack"),Before->Revision,Expected,Cost),TEXT("replayed request"));
        Check(Profile->GetProfile()->Materials.FindRef(TEXT("Shard"))==Before->Materials.FindRef(TEXT("Shard"))-Cost.FindRef(TEXT("Shard")),TEXT("cost once"));
        Check(InventoryWidget->PreviewCraft(TEXT("TemperHealth")),TEXT("replacement preview"));
        Check(InventoryWidget->ConfirmCraft(),TEXT("replacement commit"));
        auto* Crafted=Profile->GetProfile()->Items.FindByPredicate([&](const auto& I){return I.Guid==Epic.Guid;});
        Check(Crafted && Crafted->Options.FindRef(EPGStatType::Attack)==Epic.Options.FindRef(EPGStatType::Attack),TEXT("old temper removed"));
        Check(InventoryWidget->PreviewCraft(NAME_None),TEXT("masterwork preview"));
        Check(Profile->PreviewCraft(Epic.Guid,NAME_None,Expected,Cost,Reason),TEXT("masterwork quote"));
        auto* CraftPlayer=Cast<APGCharacterPlayer>(GetPawn());
        auto* ASC=CraftPlayer ? CraftPlayer->GetPGAbilitySystemComponent() : nullptr;
        const float Health=ASC ? ASC->GetHealth() : 0;
        TMap<EPGStatType,float> ExpectedStats;
        if (ASC)
            for (const auto& Pair : Expected.MasterworkBonuses)
            {
                const auto Detail=ASC->GetStatDetails(Pair.Key);
                const int32 Delta=Expected.Options.FindRef(Pair.Key)-Crafted->Options.FindRef(Pair.Key);
                ExpectedStats.Add(Pair.Key,ASC->GetStatDetails(Pair.Key,Detail.Profile+Delta).Expected);
            }
        Check(InventoryWidget->ConfirmCraft(),TEXT("masterwork commit"));
        if (ASC)
        {
            for (const auto& Pair : ExpectedStats) Check(FMath::IsNearlyEqual(ASC->GetStatDetails(Pair.Key).Current,Pair.Value),TEXT("GAS matches preview"));
            Check(ASC->GetHealth()==Health,TEXT("upgrade does not heal"));
        }
        Check(InventoryWidget->PreviewImprint(TEXT("Bloodletting")),TEXT("crafted imprint preview"));
        Check(InventoryWidget->ConfirmImprint(),TEXT("crafted imprint"));
        Crafted=Profile->GetProfile()->Items.FindByPredicate([&](const auto& I){return I.Guid==Epic.Guid;});
        Check(Crafted && Crafted->MasterworkLevel==1 && Crafted->Temper.Id==TEXT("TemperHealth") && Crafted->OriginalRarity==EPGItemRarity::Epic,TEXT("imprint preserves upgrades"));
        const UPGProfileSave* Latest=nullptr;
        for (int32 Index=0;Index<2;++Index)
        {
            auto* Loaded=Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(TEXT("PGTest_")+TestName.Left(40)+TEXT("_")+FString::FromInt(Index),0));
            if (Loaded && (!Latest || Loaded->Revision>Latest->Revision)) Latest=Loaded;
        }
        Check(Latest && Latest->Materials.OrderIndependentCompareEqual(Profile->GetProfile()->Materials) && Latest->Items.ContainsByPredicate([&](const auto& I)
            {return I.Guid==Epic.Guid && I.MasterworkLevel==1 && I.Temper.Id==TEXT("TemperHealth") && I.Options.OrderIndependentCompareEqual(Crafted->Options);}),TEXT("atomic disk reload"));
        const FName CaptureRecipe=FParse::Param(FCommandLine::Get(),TEXT("PGTemperPreview")) ? FName(TEXT("TemperAttack")) : NAME_None;
        Check(InventoryWidget->PreviewCraft(CaptureRecipe),TEXT("capture crafting preview"));
        if (FParse::Param(FCommandLine::Get(),TEXT("PGCraftCombat")))
        {
            Check(Profile->PreviewCraft(Epic.Guid,NAME_None,Expected,Cost,Reason),TEXT("combat quote"));
            APGStageManager* Stage=nullptr;
            for (TActorIterator<APGStageManager> It(GetWorld());It;++It) {Stage=*It;break;}
            Check(Stage!=nullptr,TEXT("combat stage")); const int64 Revision=Profile->GetProfile()->Revision;
            if (Stage) Stage->ReadyForNextStage();
            Check(!Profile->Craft(Epic.Guid,NAME_None,Revision,Expected,Cost),TEXT("combat rejected"));
            Check(Profile->GetProfile()->Revision==Revision,TEXT("combat preserves state"));
        }
        UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe CRAFT failures=%d"),Failures); return;
    }
    if (Action==TEXT("verifyImprint"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){PGInventoryProbe(TEXT("imprint"));return false;}),2.f);
        return;
    }
    if (Action==TEXT("imprint"))
    {
        int32 Failures=0;
        auto Check=[&](bool OK,const TCHAR* Name){if (!OK) {++Failures;UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe IMPRINT FAIL %s"),Name);}};
        if (!InventoryWidget || !Catalog->FindItem(15008)) {UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe IMPRINT missing fixture"));return;}
        auto Generate=[&](int32 Id)
        {
            FPGItemInstance Item; FString Error; FRandomStream Random(Id);
            const auto* Def=Catalog->FindItem(Id);
            const auto* Rule=Catalog->RarityRules.FindByPredicate([&](const auto& R){return R.Rarity==Def->Rarity;});
            Check(PGItemGeneration::Generate(*Def,*Rule,Random,Item,Error),TEXT("generate"));
            Catalog->InitializeLegendary(Item); Check(Profile->TryPickup(Item),TEXT("pickup")); return Item;
        };
        const auto Epic=Generate(15007); TArray<FGuid> Targets;
        for (int32 Index=0;Index<5;++Index) Targets.Add(Generate(Index<3 ? 15008 : 15009).Guid);
        TMap<FName,int64> Yield; FString Reason;
        Check(Profile->PreviewSalvage(Targets,false,Yield,Reason),TEXT("salvage preview"));
        const int64 BeforeSalvage=Profile->GetProfile()->Revision;
        Profile->bInjectSaveFailure=true;
        Check(!Profile->Salvage(Targets,false,BeforeSalvage,Yield),TEXT("salvage failure")); Profile->bInjectSaveFailure=false;
        Check(!Profile->GetProfile()->LegendaryRecipes.Contains(TEXT("Bloodletting")),TEXT("no unlock before save"));
        Check(Profile->Salvage(Targets,false,BeforeSalvage,Yield),TEXT("salvage retry"));
        Check(Profile->GetProfile()->LegendaryRecipes.Contains(TEXT("Bloodletting")),TEXT("unlocked"));
        InventoryWidget->SelectItem(Epic.Guid);
        Check(InventoryWidget->PreviewImprint(TEXT("Bloodletting")),TEXT("preview"));
        InventoryWidget->HandleEscape(); Check(!InventoryWidget->HasPendingEdit(),TEXT("escape cancels"));
        Check(InventoryWidget->PreviewImprint(TEXT("Bloodletting")),TEXT("preview again"));
        auto* Before=Profile->GetProfile();
        FPGItemInstance ExpectedItem; TMap<FName,int64> ExpectedCost;
        Check(Profile->PreviewImprint(Epic.Guid,TEXT("Bloodletting"),ExpectedItem,ExpectedCost,Reason),TEXT("expected quote"));
        Check(!Profile->Imprint(Epic.Guid,TEXT("Bloodletting"),Before->Revision-1,ExpectedItem.LegendaryPercent,ExpectedCost),TEXT("stale quote rejected"));
        Check(!Profile->Imprint(Epic.Guid,TEXT("Bloodletting"),Before->Revision,ExpectedItem.LegendaryPercent+1,ExpectedCost),TEXT("changed strength rejected"));
        auto WrongCost=ExpectedCost; WrongCost.Add(TEXT("Shard"),1);
        Check(!Profile->Imprint(Epic.Guid,TEXT("Bloodletting"),Before->Revision,ExpectedItem.LegendaryPercent,WrongCost),TEXT("changed cost rejected"));
        Profile->bInjectSaveFailure=true; Check(!InventoryWidget->ConfirmImprint(),TEXT("save failure")); Profile->bInjectSaveFailure=false;
        Check(Profile->GetProfile()==Before && InventoryWidget->HasPendingEdit(),TEXT("failure preserves snapshot and draft"));
        Check(InventoryWidget->ConfirmImprint(),TEXT("retry"));
        Check(!InventoryWidget->ConfirmImprint(),TEXT("double confirm rejected"));
        auto* Item=Profile->GetProfile()->Items.FindByPredicate([&](const auto& I){return I.Guid==Epic.Guid;});
        Check(Item && Item->Rarity==EPGItemRarity::Legendary && Item->OriginalRarity==EPGItemRarity::Epic && Item->Options.OrderIndependentCompareEqual(Epic.Options),TEXT("identity and options preserved"));
        Check(Profile->GetProfile()->Materials.FindRef(TEXT("Shard"))==Before->Materials.FindRef(TEXT("Shard"))-20,TEXT("cost once"));
        Check(Profile->Equip(Epic.Guid),TEXT("equip imprint"));
        if (auto* ImprintPlayer=Cast<APGCharacterPlayer>(GetPawn()))
            Check(ImprintPlayer->GetPGAbilitySystemComponent()->GetPerkPercent(EPGCombatPerk::Bleed)==Profile->GetEffectivePerk(EPGCombatPerk::Bleed),TEXT("actual ASC matches equipment"));
        Check(InventoryWidget->PreviewImprint(TEXT("Impact")),TEXT("replacement preview"));
        Check(InventoryWidget->ConfirmImprint(),TEXT("replacement commit"));
        Check(Profile->GetEffectivePerk(EPGCombatPerk::Bleed)==0 && Profile->GetEffectivePerk(EPGCombatPerk::Shockwave)==20,TEXT("old effect removed"));
        const UPGProfileSave* Latest=nullptr;
        for (int32 Index=0;Index<2;++Index)
        {
            auto* Loaded=Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(TEXT("PGTest_")+TestName.Left(40)+TEXT("_")+FString::FromInt(Index),0));
            if (Loaded && (!Latest || Loaded->Revision>Latest->Revision)) Latest=Loaded;
        }
        Check(Latest && Latest->LegendaryRecipes.Contains(TEXT("Bloodletting")) && Latest->Items.ContainsByPredicate([&](const auto& I){return I.Guid==Epic.Guid && I.LegendaryEffect==TEXT("Impact");}),TEXT("atomic disk reload"));
        // One more salvage funds the final non-mutating preview captured by the renderer.
        const auto Extra=Generate(15008);
        Check(Profile->PreviewSalvage({Extra.Guid},false,Yield,Reason),TEXT("refill preview"));
        Check(Profile->Salvage({Extra.Guid},false,Profile->GetProfile()->Revision,Yield),TEXT("refill"));
        InventoryWidget->SelectItem(Epic.Guid);
        Check(InventoryWidget->PreviewImprint(TEXT("Bloodletting")),TEXT("capture replacement preview"));
        if (FParse::Param(FCommandLine::Get(),TEXT("PGImprintCombat")))
        {
            APGStageManager* Stage=nullptr;
            for (TActorIterator<APGStageManager> It(GetWorld());It;++It) {Stage=*It;break;}
            Check(Stage!=nullptr,TEXT("combat stage"));
            const int64 Revision=Profile->GetProfile()->Revision;
            if (Stage) Stage->ReadyForNextStage();
            Check(!Profile->Imprint(Epic.Guid,TEXT("Bloodletting"),Revision,ExpectedItem.LegendaryPercent,ExpectedCost),TEXT("combat confirmation rejected"));
            Check(Profile->GetProfile()->Revision==Revision,TEXT("combat preserves state"));
        }
        UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe IMPRINT failures=%d"),Failures);
        return;
    }
    if (Action==TEXT("verifySalvage"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){ PGInventoryProbe(TEXT("salvage")); return false; }),2.f);
        return;
    }
    if (Action==TEXT("salvage"))
    {
        int32 Failures=0;
        auto Check=[&](bool OK,const TCHAR* Name){if (!OK) {++Failures;UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe SALVAGE FAIL %s"),Name);}};
        if (!InventoryWidget) {UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe SALVAGE no widget"));return;}
        TArray<FGuid> Bag;
        for (const auto& Item : Profile->GetProfile()->Items)
            if (!Profile->GetProfile()->Equipment.FindKey(Item.Guid)) Bag.Add(Item.Guid);
        if (Bag.Num()<3) {UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe SALVAGE fixture too small"));return;}
        Check(Profile->SetItemMark(Bag[0],false,true),TEXT("mark first junk"));
        Check(Profile->SetItemMark(Bag[1],false,true),TEXT("mark second junk"));
        Check(Profile->SetItemMark(Bag[2],true,false),TEXT("protect favorite"));
        const TArray<FGuid> Targets={Bag[0],Bag[1]};
        TMap<FName,int64> Rewards; FString Reason;
        Check(Profile->PreviewSalvage(Targets,true,Rewards,Reason),TEXT("preview batch"));
        int64 Revision=Profile->GetProfile()->Revision;
        const int32 Count=Profile->GetProfile()->Items.Num();
        const auto Before=Profile->GetProfile()->Materials;
        Check(!Profile->Salvage({Bag[2]},false,Revision,Rewards),TEXT("favorite rechecked"));
        for (const auto& Pair : Profile->GetProfile()->Equipment)
        { Check(!Profile->Salvage({Pair.Value},false,Revision,Rewards),TEXT("equipped rechecked")); break; }
        Check(InventoryWidget->PreviewSalvage(true),TEXT("UI preview"));
        Check(InventoryWidget->HasPendingEdit(),TEXT("preview pending"));
        InventoryWidget->HandleEscape();
        Check(!InventoryWidget->HasPendingEdit() && Profile->GetProfile()->Revision==Revision,TEXT("escape cancels without save"));
        Check(InventoryWidget->PreviewSalvage(true),TEXT("preview again"));
        Profile->bInjectSaveFailure=true;
        Check(!InventoryWidget->ConfirmSalvage(),TEXT("save failure"));
        Profile->bInjectSaveFailure=false;
        Check(Profile->GetProfile()->Revision==Revision && Profile->GetProfile()->Items.Num()==Count &&
            Profile->GetProfile()->Materials.OrderIndependentCompareEqual(Before) && InventoryWidget->HasPendingEdit(),TEXT("failure retains all state and preview"));
        Check(InventoryWidget->ConfirmSalvage(),TEXT("retry commits"));
        Check(!InventoryWidget->ConfirmSalvage(),TEXT("double UI confirm blocked"));
        Check(!Profile->Salvage(Targets,true,Revision,Rewards),TEXT("replayed request blocked"));
        Check(Profile->GetProfile()->Items.Num()==Count-2,TEXT("exactly two items removed"));
        for (const auto& Pair : Rewards)
            Check(Profile->GetProfile()->Materials.FindRef(Pair.Key)==Before.FindRef(Pair.Key)+Pair.Value,TEXT("exact material grant"));
        Check(Profile->HasClaimedLoot(Bag[0]) && Profile->HasClaimedLoot(Bag[1]),TEXT("pickup ledger retained"));
        const UPGProfileSave* Latest=nullptr;
        for (int32 Index=0;Index<2;++Index)
        {
            auto* Loaded=Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(TEXT("PGTest_")+TestName.Left(40)+TEXT("_")+FString::FromInt(Index),0));
            if (Loaded && (!Latest || Loaded->Revision>Latest->Revision)) Latest=Loaded;
        }
        Check(Latest && Latest->Items.Num()==Count-2 && Latest->Materials.OrderIndependentCompareEqual(Profile->GetProfile()->Materials),TEXT("disk snapshot atomic"));
        Check(Profile->SetItemMark(Bag[2],false,true),TEXT("remove protection explicitly"));
        Check(Profile->PreviewSalvage({Bag[2]},true,Rewards,Reason),TEXT("new preview"));
        Revision=Profile->GetProfile()->Revision;
        Check(Profile->SetItemMark(Bag[2],true,false),TEXT("protect after preview"));
        Check(!Profile->Salvage({Bag[2]},true,Revision,Rewards),TEXT("stale preview blocked"));
        Check(Profile->SetItemMark(Bag[2],false,true),TEXT("prepare screenshot item"));
        FPGInventoryFilter HiddenFilter; HiddenFilter.Mark=1;
        InventoryWidget->SetBagView(HiddenFilter,EPGInventorySort::Recent);
        Check(!InventoryWidget->GetSelectedItem().IsValid(),TEXT("fixture has no selection"));
        Check(InventoryWidget->PreviewSalvage(true),TEXT("batch ignores current filter without selection"));
        TFunction<bool(const TSharedRef<SWidget>&)> FindVisibleConfirm;
        FindVisibleConfirm=[&](const TSharedRef<SWidget>& W)
        {
            if (!W->GetVisibility().IsVisible()) return false;
            if (W->GetTag()==TEXT("PGInventory.SalvageConfirm")) return true;
            auto* Children=W->GetChildren();
            for (int32 I=0;I<Children->Num();++I) if (FindVisibleConfirm(Children->GetChildAt(I))) return true;
            return false;
        };
        Check(FindVisibleConfirm(InventoryWidget->TakeWidget()),TEXT("unselected batch confirmation visible"));
        InventoryWidget->HandleEscape();
        InventoryWidget->SetBagView(FPGInventoryFilter(),EPGInventorySort::Recent);
        InventoryWidget->SelectItem(Bag[2]);
        Check(InventoryWidget->PreviewSalvage(false),TEXT("individual preview"));
        if (FParse::Param(FCommandLine::Get(),TEXT("PGSalvageCombat")))
        {
            APGStageManager* Stage=nullptr;
            for (TActorIterator<APGStageManager> It(GetWorld());It;++It) {Stage=*It;break;}
            Check(Stage!=nullptr,TEXT("combat stage exists"));
            if (Stage) Stage->ReadyForNextStage();
            const int64 BeforeCombat=Profile->GetProfile()->Revision;
            Check(!InventoryWidget->ConfirmSalvage(),TEXT("combat confirmation rejected"));
            Check(Profile->GetProfile()->Revision==BeforeCombat,TEXT("combat does not save salvage"));
        }
        UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe SALVAGE failures=%d"),Failures);
        PGInventoryProbe(TEXT("capture"));
        return;
    }
    if (Action==TEXT("verifyGrowthNotice"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){ PGInventoryProbe(TEXT("growthNotice")); return false; }),2.f);
        return;
    }
    if (Action==TEXT("growthNotice"))
    {
        CloseInventory();
        Profile->GrantExperience(Profile->GetProfile()->RunId,FGuid::NewGuid(),650);
        auto Failures=MakeShared<int32>(0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Failures](float)
        {
            auto* P=UPGProfileSubsystem::Get(this);
            if (!InventoryWidget || !InventoryWidget->IsStatsPanelOpen() || P->HasPendingLevelNotice())
            { ++*Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe NOTICE FAIL preparation auto open")); }
            CloseInventory();
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Failures](float)
            {
                if (InventoryWidget) { ++*Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe NOTICE FAIL repeated auto open")); }
                APGStageManager* Stage=nullptr;
                for (TActorIterator<APGStageManager> It(GetWorld());It;++It) {Stage=*It;break;}
                if (Stage) Stage->ReadyForNextStage(); else ++*Failures;
                auto* CombatProfile=UPGProfileSubsystem::Get(this);
                CombatProfile->GrantExperience(CombatProfile->GetProfile()->RunId,FGuid::NewGuid(),650);
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Failures](float)
                {
                    auto* PendingProfile=UPGProfileSubsystem::Get(this);
                    FString Reason;
                    if (InventoryWidget || !PendingProfile->HasPendingLevelNotice() || PendingProfile->CanChangeSkills(Reason))
                    { ++*Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe NOTICE FAIL combat deferred")); }
                    if (!OpenGrowthPanel() || PendingProfile->HasPendingLevelNotice())
                    { ++*Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe NOTICE FAIL combat manual open")); }
                    const auto Expected=PendingProfile->GetProfile()->Growth.Investments;
                    if (PendingProfile->ApplyInvestments(PendingProfile->GetProfile()->RunId,Expected,{{EPGStatType::Attack,1}}))
                    { ++*Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe NOTICE FAIL combat applied investment")); }
                    UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe NOTICE failures=%d"),*Failures);
                    PGInventoryProbe(TEXT("capture"));
                    return false;
                }),.4f);
                return false;
            }),.4f);
            return false;
        }),.4f);
        return;
    }
    if (Action==TEXT("verifyGrowth"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){ PGInventoryProbe(TEXT("growth")); return false; }),2.f);
        return;
    }
    if (Action==TEXT("growth"))
    {
        int32 Failures=0;
        auto Check=[&](bool OK,const TCHAR* Name){if (!OK) {++Failures;UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe GROWTH FAIL %s"),Name);}};
        auto* GrowthPlayer=Cast<APGCharacterPlayer>(GetPawn());
        auto* ASC=GrowthPlayer ? GrowthPlayer->GetPGAbilitySystemComponent() : nullptr;
        if (!InventoryWidget || !ASC) { UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe GROWTH missing fixture")); return; }
        Profile->GrantExperience(Profile->GetProfile()->RunId,FGuid::NewGuid(),650);
        Check(Profile->HasPendingLevelNotice(),TEXT("committed level notice"));
        InventoryWidget->SetStatsTab();
        Check(!Profile->HasPendingLevelNotice(),TEXT("manual open acknowledges notice"));
        TFunction<TSharedPtr<SButton>(const TSharedRef<SWidget>&,FName)> Find;
        Find=[&](const TSharedRef<SWidget>& W,FName Tag)->TSharedPtr<SButton>
        {
            if (W->GetType()==TEXT("SButton") && W->GetTag()==Tag) return StaticCastSharedRef<SButton>(W);
            auto* Children=W->GetChildren();
            for (int32 I=0;I<Children->Num();++I) if (auto B=Find(Children->GetChildAt(I),Tag)) return B;
            return nullptr;
        };
        auto Click=[&](FName Tag)
        {
            if (!InventoryWidget) {Check(false,TEXT("growth window exists"));return;}
            const auto B=Find(InventoryWidget->TakeWidget(),Tag);
            // Synthetic actions share a frame; evaluate Slate attributes as the next real frame would.
            if (B) { B->MarkPrepassAsDirty(); B->SlatePrepass(); }
            if (!B || !B->IsEnabled()) {Check(false,TEXT("growth button enabled"));return;}
            const FKeyEvent Key(EKeys::Enter,FModifierKeysState(),0,false,0,0);
            B->OnKeyDown(B->GetCachedGeometry(),Key); B->OnKeyUp(B->GetCachedGeometry(),Key);
        };
        const FName AddHealth(*FString::Printf(TEXT("PGGrowth.Add.%d"),int32(EPGStatType::Health)));
        const float Health=ASC->GetHealth(), Maximum=ASC->GetCombatStat(EPGStatType::Health);
        const int32 Points=Profile->GetProfile()->Growth.UnspentPoints;
        const auto Base=Profile->GetProfile()->Growth.Investments;
        Click(AddHealth); Click(AddHealth);
        Check(ASC->GetCombatStat(EPGStatType::Health)==Maximum && Profile->GetProfile()->Growth.UnspentPoints==Points,TEXT("draft is non destructive"));
        InventoryWidget->ToggleStatsPanel();
        Check(InventoryWidget && InventoryWidget->IsStatsPanelOpen() && InventoryWidget->HasPendingEdit(),TEXT("dirty close keeps draft"));
        if (!InventoryWidget) {UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe GROWTH failures=%d"),Failures);return;}
        InventoryWidget->HandleEscape();
        Profile->bInjectSaveFailure=true; Click(TEXT("PGGrowth.Apply")); Profile->bInjectSaveFailure=false;
        Check(Profile->GetProfile()->Growth.UnspentPoints==Points && InventoryWidget->HasPendingEdit(),TEXT("failed save retains draft and points"));
        Click(TEXT("PGGrowth.Apply"));
        Check(Profile->GetProfile()->Growth.UnspentPoints==Points-2 && !InventoryWidget->HasPendingEdit(),TEXT("retry spends once"));
        Check(FMath::IsNearlyEqual(ASC->GetCombatStat(EPGStatType::Health),Maximum+2*Catalog->LevelRules.InvestmentRules[EPGStatType::Health].AmountPerPoint),TEXT("GAS applies growth"));
        Check(ASC->GetHealth()==Health,TEXT("health allocation never heals"));
        Check(!Profile->ApplyInvestments(Profile->GetProfile()->RunId,Base,{{EPGStatType::Health,2}}),TEXT("stale double apply rejected"));
        Profile->RestorePlayer(GrowthPlayer); Profile->RestorePlayer(GrowthPlayer);
        Check(ASC->GetHealth()==Health && Profile->GetProfile()->Growth.UnspentPoints==Points-2,TEXT("restore does not heal or spend"));
        Click(AddHealth);
        InventoryWidget->ToggleRightPanel(); InventoryWidget->SetStatsTab();
        Check(InventoryWidget->HasPendingEdit(),TEXT("panel switch retains draft"));
        UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe GROWTH failures=%d"),Failures);
        PGInventoryProbe(TEXT("capture"));
        return;
    }
    if (Action==TEXT("verifyDesign"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){ PGInventoryProbe(TEXT("design")); return false; }),2.f);
        return;
    }
    if (Action==TEXT("design"))
    {
        if (!InventoryWidget) return;
        int32 Failures=0;
        auto Check=[&](bool OK,const TCHAR* Name){if (!OK) {++Failures;UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe DESIGN FAIL %s"),Name);}};
        TFunction<bool(const TSharedRef<SWidget>&,const FString&)> Contains;
        Contains=[&](const TSharedRef<SWidget>& W,const FString& Label)
        {
            if (W->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(Label)) return true;
            auto* Children=W->GetChildren();
            for (int32 I=0;I<Children->Num();++I) if (Contains(Children->GetChildAt(I),Label)) return true;
            return false;
        };
        TFunction<TSharedPtr<SButton>(const TSharedRef<SWidget>&,const FString&)> Find;
        Find=[&](const TSharedRef<SWidget>& W,const FString& Label)->TSharedPtr<SButton>
        {
            if (W->GetType()==TEXT("SButton") && Contains(W,Label) &&
                (Label!=TEXT("닫기") || W->GetTag()==TEXT("PGInventory.CloseRight"))) return StaticCastSharedRef<SButton>(W);
            auto* Children=W->GetChildren();
            for (int32 I=0;I<Children->Num();++I) if (auto Button=Find(Children->GetChildAt(I),Label)) return Button;
            return nullptr;
        };
        auto Click=[&](const FString& Label)
        {
            auto Button=Find(InventoryWidget->TakeWidget(),Label);
            if (!Button || !Button->IsEnabled()) {Check(false,*Label);return;}
            const FKeyEvent Key(EKeys::Enter,FModifierKeysState(),0,false,0,0);
            Button->OnKeyDown(Button->GetCachedGeometry(),Key);
            Button->OnKeyUp(Button->GetCachedGeometry(),Key);
        };
        InventoryWidget->SetCharacterTab();
        Check(Contains(InventoryWidget->TakeWidget(),TEXT("검술 · 장비 · 강화 공유")),TEXT("default character preview has details"));
        const FName Original=Profile->GetProfile()->CharacterId;
        for (const auto& Ref:Catalog->PlayableCharacters)
            if (const auto* Identity=Ref.LoadSynchronous(); Identity && Identity->Id!=Original)
            {
                Click(Identity->DisplayName.ToString());
                Check(Profile->GetProfile()->CharacterId==Original,TEXT("portrait preview does not save"));
                Profile->bInjectSaveFailure=true;
                Click(TEXT("이 캐릭터와 함께하기"));
                Profile->bInjectSaveFailure=false;
                Check(Profile->GetProfile()->CharacterId==Original,TEXT("failed character save preserves selection"));
                Click(TEXT("이 캐릭터와 함께하기"));
                Check(Profile->GetProfile()->CharacterId==Identity->Id,TEXT("character confirmation saves"));
                break;
            }
        InventoryWidget->SetBuildTab(true);
        const auto OriginalSkills=Profile->GetProfile()->CustomActiveSkills;
        auto EquippedPair=[this]()
        {
            const auto* Player=Cast<APGCharacterPlayer>(GetPawn());
            TArray<int32> Skills;
            for (int32 Index=0;Index<PGPlayerSkillSlots::Count;++Index)
                if (const auto* Skill=Player && Player->GetSkillHandler() ? Player->GetSkillHandler()->GetSkillData(PGPlayerSkillSlots::Get(Index)) : nullptr) Skills.Add(Skill->SkillId);
            return Skills;
        };
        const auto EquippedBefore=EquippedPair();
        Click(TEXT("슬롯 2에 배치"));
        Check(Profile->GetProfile()->CustomActiveSkills==OriginalSkills,TEXT("skill draft does not save"));
        Click(TEXT("닫기"));
        Check(InventoryWidget!=nullptr,TEXT("dirty close stays open"));
        Click(TEXT("계속 편집"));
        Profile->bInjectSaveFailure=true;
        Click(TEXT("변경 적용"));
        Profile->bInjectSaveFailure=false;
        Check(Profile->GetProfile()->CustomActiveSkills==OriginalSkills,TEXT("failed skill save preserves profile"));
        Check(EquippedPair()==EquippedBefore,TEXT("failed skill save preserves runtime slots"));
        Click(TEXT("변경 적용"));
        const auto Result=Profile->GetProfile()->CustomActiveSkills;
        Check(Catalog->IsValidActiveSelection(Result),TEXT("four unique skills saved after retry"));
        Check(Result!=OriginalSkills,TEXT("retry commits the draft"));
        Check(EquippedPair()==Result,TEXT("saved draft applies runtime slots"));
        Click(TEXT("닫기"));
        Check(!InventoryWidget && !IsMoveInputIgnored(),TEXT("applied close releases input"));
        ToggleInventory(); InventoryWidget->SetBuildTab(true);
        Click(TEXT("슬롯 1에 배치"));
        Click(TEXT("슬롯 2에 배치"));
        Click(TEXT("닫기"));
        Check(!InventoryWidget,TEXT("swapping back to equipped pair leaves no dirty prompt"));
        if (!InventoryWidget) ToggleInventory();
        InventoryWidget->SetBuildTab(true);
        Click(TEXT("슬롯 1에 배치"));
        Click(TEXT("닫기"));
        Check(InventoryWidget!=nullptr,TEXT("swap close asks confirmation"));
        Click(TEXT("변경 취소 후 닫기"));
        Check(!InventoryWidget && Profile->GetProfile()->CustomActiveSkills==Result,TEXT("discard closes without saving"));
        if (!InventoryWidget) ToggleInventory();
        InventoryWidget->SetBuildTab(true);
        Click(TEXT("슬롯 1에 배치"));
        Click(TEXT("닫기"));
        Profile->bInjectSaveFailure=true;
        Click(TEXT("적용 후 닫기"));
        Profile->bInjectSaveFailure=false;
        Check(InventoryWidget && Profile->GetProfile()->CustomActiveSkills==Result,TEXT("failed apply and close keeps draft open"));
        Click(TEXT("적용 후 닫기"));
        Check(!InventoryWidget && !IsMoveInputIgnored() && !IsPaused(),TEXT("apply and close restores input"));
        const auto Swapped=Profile->GetProfile()->CustomActiveSkills;
        Check(Swapped.Num()==4 && Swapped[0]==Result[1] && Swapped[1]==Result[0] && Swapped[2]==Result[2] && Swapped[3]==Result[3],TEXT("duplicate assignment swaps both slots"));
        ToggleInventory(); InventoryWidget->SetBuildTab(true);
        // Leave the pending-close layout visible for a real, arranged 720p capture.
        Click(TEXT("슬롯 2에 배치"));
        Click(TEXT("닫기"));
        UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe DESIGN failures=%d"),Failures);
        PGInventoryProbe(TEXT("capture"));
        return;
    }
    if (Action==TEXT("verify"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){ PGInventoryProbe(TEXT("check")); return false; }),2.f);
        return;
    }
    if (Action==TEXT("verifyStates"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){ PGInventoryProbe(TEXT("states")); return false; }),2.f);
        return;
    }
    if (Action==TEXT("states"))
    {
        int32 Failures=0;
        auto Check=[&Failures](bool bOK,const TCHAR* Name)
        { if (!bOK) { ++Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe FAIL %s"),Name); } };
        Check(InventoryWidget && !IsMoveInputIgnored() && !IsPaused(),TEXT("preparation remains unpaused"));
        CloseInventory();
        APGStageManager* Stage=nullptr;
        for (TActorIterator<APGStageManager> It(GetWorld());It;++It) { Stage=*It;break; }
        Check(Stage!=nullptr,TEXT("stage fixture"));
        if (Stage)
        {
            Stage->ReadyForNextStage();
            ToggleInventory();
            Check(InventoryWidget && !IsPaused() && !IsMoveInputIgnored(),TEXT("combat overlay keeps world running"));
            FString Reason;
            Check(!Profile->CanChangeSkills(Reason) && !Reason.IsEmpty(),TEXT("combat loadout changes have a reason"));
            FPGStagePresentation View; View.Owner=Stage; View.Status=FText::FromString(TEXT("UI 입력 전환 검사"));
            auto* Messages=UPGMessageManager::Get(this);
            Messages->SendMessage(EPGUIMessageType::StagePresentation,&View);
            Check(!InventoryWidget && !IsPaused() && IsMoveInputIgnored(),TEXT("stage modal supersedes inventory without stale pause"));
            View.bClose=true; Messages->SendMessage(EPGUIMessageType::StagePresentation,&View);
            Check(!IsMoveInputIgnored() && !IsPaused() && !UPGUIManager::Get(this)->IsWindowOpen(),TEXT("stage modal restores input"));
            ToggleInventory();
            if (InventoryWidget) InventoryWidget->SetBuildTab(true);
        }
        UE_LOG(LogTemp,Log,TEXT("PGInventoryProbe STATES failures=%d"),Failures);
        PGInventoryProbe(TEXT("capture"));
        return;
    }
    if (Action==TEXT("open") || Action==TEXT("full") || Action==TEXT("empty") || Action==TEXT("build") || Action==TEXT("character") || Action==TEXT("stats") || Action==TEXT("failure"))
    {
        CloseInventory();
        if (FParse::Param(FCommandLine::Get(),TEXT("PGEquipmentExpansion")))
        {
            int32 Failures=0;
            for (int32 Id=15020;Id<=15041;++Id)
            {
                const auto* Def=Catalog->FindItem(FMath::Min(Id,15040));
                const auto* Rule=Def ? Catalog->RarityRules.FindByPredicate([&](const auto& R){return R.Rarity==Def->Rarity;}) : nullptr;
                FPGItemInstance Item; FString Error; FRandomStream Random(Id);
                if (!Rule || !PGItemGeneration::Generate(*Def,*Rule,Random,Item,Error) || !Profile->TryPickup(Item)) {++Failures;continue;}
                if (Id==15041)
                { if (!Profile->EquipToSlot(Item.Guid,EPGEquipmentSlot::RingRight)) ++Failures; }
                else if ((Id-15020)%3==2 && !Profile->EquipToSlot(Item.Guid,Def->Slot)) ++Failures;
            }
            auto* ExpansionPlayer=Cast<APGCharacterPlayer>(GetPawn());
            auto* ASC=ExpansionPlayer ? ExpansionPlayer->GetPGAbilitySystemComponent() : nullptr;
            TMap<EPGStatType,int32> Expected;
            for (const auto& Item : Profile->GetProfile()->Items)
                if (Profile->GetProfile()->Equipment.FindKey(Item.Guid))
                    for (const auto& Pair : Item.Options) Expected.FindOrAdd(Pair.Key)+=Pair.Value;
            if (!ASC) ++Failures;
            else for (const auto& Pair : Expected)
                if (!FMath::IsNearlyEqual(ASC->GetStatDetails(Pair.Key).Profile,float(Pair.Value))) ++Failures;
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe EXPANSION failures=%d slots=%d"),Failures,Profile->GetProfile()->Equipment.Num());
        }
        else if (Action==TEXT("empty"))
        {
            Profile->Unequip(EPGEquipmentSlot::Weapon); Profile->Unequip(EPGEquipmentSlot::Accessory);
            const auto Items=Profile->GetProfile()->Items;
            for (const auto& Item : Items) Profile->Discard(Item.Guid);
        }
        else
        {
            const int32 Target=Action==TEXT("full") ? Catalog->BagCapacity : FMath::Min(12,Catalog->BagCapacity);
            for (int32 I=Profile->GetBagCount();I<Target && !Catalog->Items.IsEmpty();++I)
            {
                const auto& Def=Catalog->Items[I%Catalog->Items.Num()];
                FPGItemInstance Item;
                if (Def.bUseOptionPool)
                {
                    const auto* Rule=Catalog->RarityRules.FindByPredicate([&](const auto& R){ return R.Rarity==Def.Rarity; });
                    FRandomStream Random(731+I); FString Error;
                    if (!Rule || !PGItemGeneration::Generate(Def,*Rule,Random,Item,Error))
                    { UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe generation failed: %s"),*Error); return; }
                }
                else
                {
                    Item.Guid=FGuid::NewGuid(); Item.DefinitionId=Def.Id; Item.Options=Def.BaseOptions;
                    if (Def.Slot==EPGEquipmentSlot::Weapon) Item.Options.FindOrAdd(EPGStatType::Attack)+=I*3;
                }
                if (!Profile->TryPickup(Item)) break;
            }
        }
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
        ToggleInventory();
        if (!InventoryWidget) { UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe could not open inventory")); return; }
        if (Action==TEXT("build")) InventoryWidget->SetBuildTab(true);
        else if (Action==TEXT("stats")) InventoryWidget->SetStatsTab();
        else if (Action==TEXT("character"))
        {
            InventoryWidget->SetCharacterTab();
            FString PreviewId;
            if (FParse::Value(FCommandLine::Get(),TEXT("PGPortraitPreview="),PreviewId))
            {
                const UPGCharacterAppearance* Preview=nullptr;
                for (const auto& Ref:Catalog->PlayableCharacters)
                    if (const auto* Entry=Ref.LoadSynchronous(); Entry && Entry->Id.ToString()==PreviewId) Preview=Entry;
                TFunction<bool(const TSharedRef<SWidget>&)> HasLabel;
                HasLabel=[&](const TSharedRef<SWidget>& Widget)
                {
                    if (Widget->GetType()==TEXT("STextBlock") && Preview &&
                        StaticCastSharedRef<STextBlock>(Widget)->GetText().EqualTo(Preview->DisplayName)) return true;
                    auto* Children=Widget->GetChildren();
                    for (int32 I=0;I<Children->Num();++I) if (HasLabel(Children->GetChildAt(I))) return true;
                    return false;
                };
                TFunction<TSharedPtr<SButton>(const TSharedRef<SWidget>&)> FindButton;
                FindButton=[&](const TSharedRef<SWidget>& Widget)->TSharedPtr<SButton>
                {
                    if (Widget->GetType()==TEXT("SButton") && HasLabel(Widget)) return StaticCastSharedRef<SButton>(Widget);
                    auto* Children=Widget->GetChildren();
                    for (int32 I=0;I<Children->Num();++I) if (auto Button=FindButton(Children->GetChildAt(I))) return Button;
                    return nullptr;
                };
                if (auto Button=FindButton(InventoryWidget->TakeWidget()))
                {
                    const FKeyEvent Key(EKeys::Enter,FModifierKeysState(),0,false,0,0);
                    Button->OnKeyDown(Button->GetCachedGeometry(),Key);
                    Button->OnKeyUp(Button->GetCachedGeometry(),Key);
                }
                else UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe missing portrait button %s"),*PreviewId);
            }
        }
        else if (Action!=TEXT("empty"))
        {
            for (const auto& Item : Profile->GetProfile()->Items)
            {
                const auto* Def=Catalog->FindItem(Item.DefinitionId);
                if (Def && Profile->GetProfile()->Equipment.FindRef(Def->Slot)!=Item.Guid &&
                    (!FParse::Param(FCommandLine::Get(),TEXT("PGValidateOptions")) ||
                     Item.OptionRolls.ContainsByPredicate([](const auto& R){ return R.Source==EPGItemOptionSource::Affix; })))
                {
                    InventoryWidget->SelectItem(Item.Guid);
                    if (Action==TEXT("failure")) { Profile->bInjectSaveFailure=true; InventoryWidget->EquipSelected(); Profile->bInjectSaveFailure=false; }
                    break;
                }
            }
        }
        UE_LOG(LogTemp,Log,TEXT("PGInventoryProbe OPEN mode=%s items=%d ignored=%d paused=%d"),*Action,Profile->GetProfile()->Items.Num(),IsMoveInputIgnored(),IsPaused());
        PGInventoryProbe(TEXT("capture"));
        return;
    }
    if (Action==TEXT("capture"))
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
        { PGInventoryProbe(TEXT("captureReady")); return false; }),3.f);
        return;
    }
    if (Action==TEXT("captureReady"))
    {
        if (FParse::Param(FCommandLine::Get(),TEXT("PGCraftCombat")))
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe CRAFT COMBAT closed=%d"),InventoryWidget==nullptr);
        if (FParse::Param(FCommandLine::Get(),TEXT("PGImprintCombat")))
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe IMPRINT COMBAT closed=%d"),InventoryWidget==nullptr);
        if (FParse::Param(FCommandLine::Get(),TEXT("PGSalvageCombat")))
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe SALVAGE COMBAT closed=%d"),InventoryWidget==nullptr);
        if (FAssetCompilingManager::Get().GetNumRemainingAssets()>0)
        {
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
            { PGInventoryProbe(TEXT("captureReady")); return false; }),.5f);
            return;
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("PGValidatePortraits")) && InventoryWidget)
        {
            int32 Images=0, ArrangedImages=0, Failures=0;
            TMap<FString,int32> Counts;
            TMap<FName,UTexture2D*> Portraits;
            for (const auto& Ref:Catalog->PlayableCharacters)
                if (const auto* Entry=Ref.LoadSynchronous()) Portraits.Add(Entry->Id,Entry->Portrait.LoadSynchronous());
            TFunction<void(const TSharedRef<SWidget>&)> Inspect;
            Inspect=[&](const TSharedRef<SWidget>& Widget)
            {
                if (Widget->GetType()==TEXT("SImage"))
                {
                    const auto* Texture=Portraits.FindRef(Widget->GetTag());
                    if (Texture)
                    {
                        ++Images;
                        ++Counts.FindOrAdd(Texture->GetName());
                        const FIntPoint SourceSize=Texture->GetImportedSize();
                        const FVector2D DesiredSize=Widget->GetDesiredSize();
                        const FVector2D DrawSize=Widget->GetCachedGeometry().GetLocalSize();
                        const double Expected=SourceSize.Y>0 ? double(SourceSize.X)/SourceSize.Y : 0.;
                        // Offscreen scroll children are intentionally not arranged by Slate.
                        const bool Arranged=DrawSize.Y>0;
                        if (Arranged) ++ArrangedImages;
                        const bool Correct=Expected>0 && DesiredSize.Y>0 &&
                            FMath::IsNearlyEqual(DesiredSize.X/DesiredSize.Y,Expected,.001) &&
                            (!Arranged || FMath::IsNearlyEqual(DrawSize.X/DrawSize.Y,Expected,.001));
                        if (!Correct) ++Failures;
                        UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe PORTRAIT image=%s source=%dx%d brush=%s draw=%s valid=%d"),
                            *Texture->GetName(),SourceSize.X,SourceSize.Y,*DesiredSize.ToString(),*DrawSize.ToString(),Correct);
                    }
                }
                auto* Children=Widget->GetChildren();
                for (int32 I=0;I<Children->Num();++I) Inspect(Children->GetChildAt(I));
            };
            Inspect(InventoryWidget->TakeWidget());
            FString PreviewId;
            if (!Catalog->PlayableCharacters.IsEmpty())
                if (const auto* First=Catalog->PlayableCharacters[0].LoadSynchronous()) PreviewId=First->Id.ToString();
            FParse::Value(FCommandLine::Get(),TEXT("PGPortraitPreview="),PreviewId);
            if (ArrangedImages==0 || Images!=Catalog->PlayableCharacters.Num()+1 || Counts.Num()!=Catalog->PlayableCharacters.Num() ||
                Counts.FindRef(TEXT("T_")+PreviewId)!=2 || !Profile->GetProfile()->CharacterId.IsNone()) ++Failures;
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe PORTRAITS images=%d failures=%d preview=%s"),Images,Failures,*PreviewId);
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("PGValidateStats")))
        {
            int32 Values=0, Sources=0, Failures=0;
            const auto* StatOwner=Cast<APGCharacterPlayer>(GetPawn());
            const auto* Stats=StatOwner ? StatOwner->GetPGAbilitySystemComponent() : nullptr;
            TFunction<void(const TSharedRef<SWidget>&)> Inspect;
            Inspect=[&](const TSharedRef<SWidget>& Widget)
            {
                if (Widget->GetType()==TEXT("STextBlock"))
                    for (int32 Id=1;Id<int32(EPGStatType::Max);++Id)
                        if (Widget->GetTag()==FName(*FString::Printf(TEXT("PGStat.%d"),Id)))
                        {
                            ++Values;
                            const auto Stat=EPGStatType(Id);
                            if (!Stats || !StaticCastSharedRef<STextBlock>(Widget)->GetText().EqualTo(
                                PGInventoryPresentation::StatValue(Stat,Stats->GetCombatStat(Stat)))) ++Failures;
                        }
                        else if (Widget->GetTag()==FName(*FString::Printf(TEXT("PGStatSource.%d"),Id)))
                        {
                            ++Sources;
                            const FString Value=StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
                            if (!Value.Contains(TEXT("기본값")) || !Value.Contains(TEXT("런 강화")) || !Value.Contains(TEXT("기타 효과·상한 보정"))) ++Failures;
                        }
                auto* Children=Widget->GetChildren();
                for (int32 I=0;I<Children->Num();++I) Inspect(Children->GetChildAt(I));
            };
            if (InventoryWidget) Inspect(InventoryWidget->TakeWidget());
            if (Values!=7 || Sources!=7) ++Failures;
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe STATS values=%d failures=%d"),Values,Failures);
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("PGValidateOptions")))
        {
            int32 Failures=0, Matched=0;
            const auto* Item=InventoryWidget ? Profile->GetProfile()->Items.FindByPredicate(
                [&](const auto& I){ return I.Guid==InventoryWidget->GetSelectedItem(); }) : nullptr;
            TArray<FString> Texts;
            TFunction<void(const TSharedRef<SWidget>&)> Inspect;
            Inspect=[&](const TSharedRef<SWidget>& Widget)
            {
                if (Widget->GetType()==TEXT("STextBlock")) Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
                auto* Children=Widget->GetChildren();
                for (int32 I=0;I<Children->Num();++I) Inspect(Children->GetChildAt(I));
            };
            if (InventoryWidget) Inspect(InventoryWidget->TakeWidget());
            if (!Item || Item->OptionRolls.Num()<2 || !PGEquipmentRules::ValidateInstance(*Item)) ++Failures;
            if (Item) for (const auto& Roll : Item->OptionRolls)
            {
                const TCHAR* Source=Roll.Source==EPGItemOptionSource::Base ? TEXT("기본 성능") :
                    Roll.Source==EPGItemOptionSource::Implicit ? TEXT("고정 특성") : Roll.bGreater ? TEXT("◆ 상위 옵션") : TEXT("일반 옵션");
                const FString Expected=FString::Printf(TEXT("%s · %s %s (일반 범위 %s~%s)"),Source,
                    *PGInventoryPresentation::StatName(Roll.Stat).ToString(),*PGInventoryPresentation::StatValue(Roll.Stat,Roll.Value).ToString(),
                    *PGInventoryPresentation::StatValue(Roll.Stat,Roll.Min).ToString(),*PGInventoryPresentation::StatValue(Roll.Stat,Roll.Max).ToString());
                if (Texts.Contains(Expected)) ++Matched; else ++Failures;
            }
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe OPTIONS matched=%d failures=%d"),Matched,Failures);
        }
        if (InventoryWidget)
        {
            int32 Failures=0; float LeftWidth=0,RightWidth=0;
            const auto RootSize=InventoryWidget->GetCachedGeometry().GetLocalSize();
            TFunction<void(const TSharedRef<SWidget>&)> InspectPanels;
            InspectPanels=[&](const TSharedRef<SWidget>& W)
            {
                if (W->GetVisibility().IsVisible() && (W->GetTag()==TEXT("PGInventory.Left") || W->GetTag()==TEXT("PGInventory.Right")))
                {
                    const auto Size=W->GetCachedGeometry().GetLocalSize();
                    if (Size.X<=0 || Size.Y<=0) ++Failures;
                    if (W->GetTag()==TEXT("PGInventory.Left")) LeftWidth=Size.X; else RightWidth=Size.X;
                }
                auto* Children=W->GetChildren();
                for (int32 I=0;I<Children->Num();++I) InspectPanels(Children->GetChildAt(I));
            };
            InspectPanels(InventoryWidget->TakeWidget());
            int32 X=0,Y=0; GetViewportSize(X,Y);
            if (X<1600 && LeftWidth>0 && RightWidth>0) ++Failures;
            if (X>=1600 && (LeftWidth>RootSize.X*.30f || RightWidth>RootSize.X*.30f)) ++Failures;
            if (const auto* InputOwner=Cast<APGCharacterPlayer>(GetPawn()); InputOwner && InputOwner->GetInputConfig() && GetLocalPlayer())
                if (const auto* Input=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    auto CheckKeys=[&](const auto& Mappings)
                    { for (const auto& Mapping : Mappings)
                        if (Input->QueryKeysMappedToAction(Mapping.InputAction).Contains(StatsKey)) ++Failures; };
                    CheckKeys(InputOwner->GetInputConfig()->AbilityInputActions);
                    CheckKeys(InputOwner->GetInputConfig()->NativeInputActions);
                }
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe OVERLAY left=%.1f right=%.1f width=%.1f failures=%d"),LeftWidth,RightWidth,RootSize.X,Failures);
        }
        if (InventoryWidget && FParse::Param(FCommandLine::Get(),TEXT("PGCaptureDetails")))
        {
            int32 Scrolled=0;
            TFunction<void(const TSharedRef<SWidget>&)> ScrollDetails;
            ScrollDetails=[&](const TSharedRef<SWidget>& Widget)
            {
                if (Widget->GetType()==TEXT("SScrollBox"))
                { StaticCastSharedRef<SScrollBox>(Widget)->ScrollToEnd(); ++Scrolled; }
                auto* Children=Widget->GetChildren();
                for (int32 I=0;I<Children->Num();++I) ScrollDetails(Children->GetChildAt(I));
            };
            ScrollDetails(InventoryWidget->TakeWidget());
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe DETAILS scrolled=%d"),Scrolled);
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[](float)
            {
                UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe CAPTURE ready"));
                FScreenshotRequest::RequestScreenshot(TEXT("PGInventory"),true,true);
                return false;
            }),.5f);
            return;
        }
        UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe CAPTURE ready"));
        FScreenshotRequest::RequestScreenshot(TEXT("PGInventory"),true,true);
        return;
    }
    if (Action==TEXT("close")) { CloseInventory(); return; }
    if (Action==TEXT("check"))
    {
        if (!InventoryWidget) { UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe CHECK missing window")); return; }
        int32 Failures=0;
        auto Check=[&Failures](bool bOK,const TCHAR* Name)
        { if (!bOK) { ++Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe FAIL %s"),Name); } };
        Check(!IsMoveInputIgnored() && !IsPaused(),TEXT("overlay keeps world input and time"));
        FGuid Candidate;
        for (const auto& Item : Profile->GetProfile()->Items)
            if (const auto* Def=Catalog->FindItem(Item.DefinitionId); Def && Profile->GetProfile()->Equipment.FindRef(Def->Slot)!=Item.Guid) { Candidate=Item.Guid; break; }
        Check(Candidate.IsValid(),TEXT("fixture has unequipped item"));
        if (Candidate.IsValid())
        {
            const auto Equipment=Profile->GetProfile()->Equipment;
            InventoryWidget->SelectItem(Candidate);
            InventoryWidget->HandleEscape();
            Check(InventoryWidget && InventoryWidget->GetSelectedItem()==Candidate,TEXT("Escape closes detail and preserves selection"));
            InventoryWidget->SetStatsTab();
            int32 PixelsX=0,PixelsY=0; GetViewportSize(PixelsX,PixelsY);
            Check(InventoryWidget->IsStatsPanelOpen() && (PixelsX>=1600 || !InventoryWidget->IsRightPanelOpen()),TEXT("compact mode folds opposite panel"));
            InventoryWidget->ToggleRightPanel();
            Check(InventoryWidget->IsRightPanelOpen() && InventoryWidget->GetSelectedItem()==Candidate,TEXT("panel switch preserves selection"));
            const auto OwnedBefore=Profile->GetProfile()->Items;
            for (const auto Sort : {EPGInventorySort::Recent,EPGInventorySort::Slot,EPGInventorySort::Rarity,EPGInventorySort::Power})
            {
                InventoryWidget->SetBagView(FPGInventoryFilter(),Sort);
                Check(InventoryWidget->GetSelectedItem()==Candidate,TEXT("sort preserves selected GUID"));
                for (int32 I=0;I<OwnedBefore.Num();++I)
                    Check(OwnedBefore[I].Guid==Profile->GetProfile()->Items[I].Guid,TEXT("sort never rewrites acquisition order"));
            }
            FPGInventoryFilter Filter; Filter.Mark=1;
            InventoryWidget->SetBagView(Filter,EPGInventorySort::Recent);
            Check(!InventoryWidget->GetSelectedItem().IsValid(),TEXT("filtered selection clears"));
            InventoryWidget->SetBagView(FPGInventoryFilter(),EPGInventorySort::Recent);
            Profile->bInjectSaveFailure=true;
            Check(!Profile->SetItemMark(Candidate,true,false),TEXT("failed mark leaves snapshot unchanged"));
            Profile->bInjectSaveFailure=false;
            Check(Profile->SetItemMark(Candidate,true,false),TEXT("favorite save commits"));
            Check(!Profile->Discard(Candidate),TEXT("favorite blocks destructive action"));
            Check(!Profile->SetItemMark(Candidate,true,true),TEXT("marks are exclusive"));
            Check(Profile->SetItemMark(Candidate,false,true),TEXT("junk replaces favorite"));
            Filter.Mark=2; InventoryWidget->SetBagView(Filter,EPGInventorySort::Recent);
            Check(InventoryWidget->GetVisibleItems().Contains(Candidate),TEXT("junk filter includes marked item"));
            InventoryWidget->SetBagView(FPGInventoryFilter(),EPGInventorySort::Recent);
            InventoryWidget->SelectItem(Candidate);
            Check(Profile->GetProfile()->Equipment.OrderIndependentCompareEqual(Equipment),TEXT("selection does not equip"));
            const auto* Before=Profile->GetProfile();
            Profile->bInjectSaveFailure=true;
            Check(!InventoryWidget->EquipSelected(),TEXT("failed save rejects equip"));
            Check(Profile->GetProfile()==Before && InventoryWidget->GetSelectedItem()==Candidate,TEXT("failed equip preserves snapshot and selection"));
            Profile->bInjectSaveFailure=false;
            Check(InventoryWidget->EquipSelected(),TEXT("equip retry commits"));
            Check(!InventoryWidget->EquipSelected(),TEXT("equipped action cannot repeat"));
            Check(!Profile->SetItemMark(Candidate,false,true),TEXT("equipped junk rejected"));
            Check(!InventoryWidget->DiscardSelected(),TEXT("equipped item cannot be discarded"));
            const auto* Item=Profile->GetProfile()->Items.FindByPredicate([Candidate](const auto& Entry){return Entry.Guid==Candidate;});
            const auto* Def=Item ? Catalog->FindItem(Item->DefinitionId) : nullptr;
            if (Def) Profile->Unequip(Def->Slot);
            // Refresh the presentation from the real event before a subsequent UI action.
            InventoryWidget->SelectItem(Candidate);
            Check(!InventoryWidget->DiscardSelected(),TEXT("first discard requires confirmation"));
            Profile->bInjectSaveFailure=true;
            Check(!InventoryWidget->DiscardSelected(),TEXT("failed discard retains item"));
            Check(Profile->GetProfile()->Items.ContainsByPredicate([Candidate](const auto& Entry){return Entry.Guid==Candidate;}),TEXT("failed discard item retained"));
            Profile->bInjectSaveFailure=false;
            Check(!InventoryWidget->DiscardSelected(),TEXT("retry requires renewed confirmation"));
            Check(InventoryWidget->DiscardSelected(),TEXT("confirmed discard commits"));
        }
        auto SendKey=[](FKey Key)
        {
            const FModifierKeysState Modifiers;
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,Modifiers,0,false,0,0));
            FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key,Modifiers,0,false,0,0));
        };
        SendKey(EKeys::Escape);
        Check(!InventoryWidget && !IsMoveInputIgnored() && !IsPaused(),TEXT("Escape restores gameplay"));
        for (int32 I=0;I<20;++I) { ToggleInventory(); Check(InventoryWidget!=nullptr,TEXT("reopen")); CloseInventory(); }
        Check(!IsMoveInputIgnored() && !IsPaused(),TEXT("20 cycles leave no move or pause locks"));
        // An independently acquired move lock must survive a modal window.
        ToggleInventory(); SetIgnoreMoveInput(true); CloseInventory();
        Check(IsMoveInputIgnored(),TEXT("external move lock survives close")); SetIgnoreMoveInput(false);
        ToggleInventory();
        // Focus paths become routable after the new Slate tree has been arranged.
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Failures,SendKey](float) mutable
        {
            SendKey(EKeys::I);
            if (InventoryWidget || IsMoveInputIgnored())
            { ++Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe FAIL I closes overlay")); }
            CloseInventory(); ToggleInventory();
            UE_LOG(LogTemp,Log,TEXT("PGInventoryProbe CHECK failures=%d"),Failures);
            PGInventoryProbe(TEXT("capture"));
            return false;
        }),.2f);
    }
#endif
}

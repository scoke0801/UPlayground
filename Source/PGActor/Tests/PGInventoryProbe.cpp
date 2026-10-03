#include "PGActor/Controllers/PGPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGUI/Widget/Window/PGUIInventory.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#endif

void APGPlayerController::PGInventoryProbe(FString Action)
{
#if !UE_BUILD_SHIPPING
    FString TestName;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),TestName) || !TestName.StartsWith(TEXT("UI_")))
    { UE_LOG(LogTemp,Warning,TEXT("PGInventoryProbe requires an isolated -PGTestProfile=UI_*")); return; }
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->GetProfile() || !Profile->GetCatalog()) return;
    const auto* Catalog=Profile->GetCatalog();
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
        Check(InventoryWidget && IsMoveInputIgnored() && !IsPaused(),TEXT("preparation remains unpaused"));
        CloseInventory();
        APGStageManager* Stage=nullptr;
        for (TActorIterator<APGStageManager> It(GetWorld());It;++It) { Stage=*It;break; }
        Check(Stage!=nullptr,TEXT("stage fixture"));
        if (Stage)
        {
            Stage->ReadyForNextStage();
            ToggleInventory();
            Check(InventoryWidget && IsPaused() && IsMoveInputIgnored(),TEXT("combat inventory pauses world"));
            FPGStagePresentation View; View.Owner=Stage; View.Status=FText::FromString(TEXT("UI 입력 전환 검사"));
            auto* Messages=UPGMessageManager::Get(this);
            Messages->SendMessage(EPGUIMessageType::StagePresentation,&View);
            Check(!InventoryWidget && !IsPaused() && IsMoveInputIgnored(),TEXT("stage modal supersedes inventory without stale pause"));
            View.bClose=true; Messages->SendMessage(EPGUIMessageType::StagePresentation,&View);
            Check(!IsMoveInputIgnored() && !IsPaused() && !UPGUIManager::Get(this)->IsWindowOpen(),TEXT("stage modal restores input"));
            ToggleInventory();
        }
        UE_LOG(LogTemp,Log,TEXT("PGInventoryProbe STATES failures=%d"),Failures);
        PGInventoryProbe(TEXT("capture"));
        return;
    }
    if (Action==TEXT("open") || Action==TEXT("full") || Action==TEXT("empty") || Action==TEXT("build") || Action==TEXT("failure"))
    {
        CloseInventory();
        if (Action==TEXT("empty"))
        {
            Profile->Unequip(EPGEquipmentSlot::Weapon); Profile->Unequip(EPGEquipmentSlot::Accessory);
            const auto Items=Profile->GetProfile()->Items;
            for (const auto& Item : Items) Profile->Discard(Item.Guid);
        }
        else
        {
            const int32 Target=Action==TEXT("full") ? Catalog->BagCapacity : FMath::Min(12,Catalog->BagCapacity);
            for (int32 I=Profile->GetProfile()->Items.Num();I<Target && !Catalog->Items.IsEmpty();++I)
            {
                const auto& Def=Catalog->Items[I%Catalog->Items.Num()];
                FPGItemInstance Item; Item.Guid=FGuid::NewGuid(); Item.DefinitionId=Def.Id; Item.Options=Def.BaseOptions;
                if (Def.Slot==EPGEquipmentSlot::Weapon) Item.Options.FindOrAdd(EPGStatType::Attack)+=I*3;
                if (!Profile->TryPickup(Item)) break;
            }
        }
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
        ToggleInventory();
        if (!InventoryWidget) { UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe could not open inventory")); return; }
        if (Action==TEXT("build")) InventoryWidget->SetBuildTab(true);
        else if (Action!=TEXT("empty"))
        {
            for (const auto& Item : Profile->GetProfile()->Items)
            {
                const auto* Def=Catalog->FindItem(Item.DefinitionId);
                if (Def && Profile->GetProfile()->Equipment.FindRef(Def->Slot)!=Item.Guid)
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
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[](float)
        { FScreenshotRequest::RequestScreenshot(TEXT("PGInventory"),true,true); return false; }),3.f);
        return;
    }
    if (Action==TEXT("close")) { CloseInventory(); return; }
    if (Action==TEXT("check"))
    {
        if (!InventoryWidget) { UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe CHECK missing window")); return; }
        int32 Failures=0;
        auto Check=[&Failures](bool bOK,const TCHAR* Name)
        { if (!bOK) { ++Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe FAIL %s"),Name); } };
        Check(IsMoveInputIgnored(),TEXT("modal move lock"));
        FGuid Candidate;
        for (const auto& Item : Profile->GetProfile()->Items)
            if (const auto* Def=Catalog->FindItem(Item.DefinitionId); Def && Profile->GetProfile()->Equipment.FindRef(Def->Slot)!=Item.Guid) { Candidate=Item.Guid; break; }
        Check(Candidate.IsValid(),TEXT("fixture has unequipped item"));
        if (Candidate.IsValid())
        {
            const auto Equipment=Profile->GetProfile()->Equipment;
            InventoryWidget->SelectItem(Candidate);
            Check(Profile->GetProfile()->Equipment.OrderIndependentCompareEqual(Equipment),TEXT("selection does not equip"));
            const auto* Before=Profile->GetProfile();
            Profile->bInjectSaveFailure=true;
            Check(!InventoryWidget->EquipSelected(),TEXT("failed save rejects equip"));
            Check(Profile->GetProfile()==Before && InventoryWidget->GetSelectedItem()==Candidate,TEXT("failed equip preserves snapshot and selection"));
            Profile->bInjectSaveFailure=false;
            Check(InventoryWidget->EquipSelected(),TEXT("equip retry commits"));
            Check(!InventoryWidget->EquipSelected(),TEXT("equipped action cannot repeat"));
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
            { ++Failures; UE_LOG(LogTemp,Error,TEXT("PGInventoryProbe FAIL I closes UIOnly window")); }
            CloseInventory(); ToggleInventory();
            UE_LOG(LogTemp,Log,TEXT("PGInventoryProbe CHECK failures=%d"),Failures);
            PGInventoryProbe(TEXT("capture"));
            return false;
        }),.2f);
    }
#endif
}

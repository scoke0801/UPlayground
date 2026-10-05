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
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Engine/Texture2D.h"
#include "AssetCompilingManager.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
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
            if (W->GetType()==TEXT("SButton") && Contains(W,Label)) return StaticCastSharedRef<SButton>(W);
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
    if (Action==TEXT("open") || Action==TEXT("full") || Action==TEXT("empty") || Action==TEXT("build") || Action==TEXT("character") || Action==TEXT("failure"))
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
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
        { PGInventoryProbe(TEXT("captureReady")); return false; }),3.f);
        return;
    }
    if (Action==TEXT("captureReady"))
    {
        if (FAssetCompilingManager::Get().GetNumRemainingAssets()>0)
        {
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
            { PGInventoryProbe(TEXT("captureReady")); return false; }),.5f);
            return;
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("PGValidatePortraits")) && InventoryWidget)
        {
            int32 Images=0, Failures=0;
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
                        const bool Correct=Expected>0 && DesiredSize.Y>0 && DrawSize.Y>0 &&
                            FMath::IsNearlyEqual(DesiredSize.X/DesiredSize.Y,Expected,.001) &&
                            FMath::IsNearlyEqual(DrawSize.X/DrawSize.Y,Expected,.001);
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
            if (Images!=Catalog->PlayableCharacters.Num()+1 || Counts.Num()!=Catalog->PlayableCharacters.Num() ||
                Counts.FindRef(TEXT("T_")+PreviewId)!=2 || !Profile->GetProfile()->CharacterId.IsNone()) ++Failures;
            UE_LOG(LogTemp,Display,TEXT("PGInventoryProbe PORTRAITS images=%d failures=%d preview=%s"),Images,Failures,*PreviewId);
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

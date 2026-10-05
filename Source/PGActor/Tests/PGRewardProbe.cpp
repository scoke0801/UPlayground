#include "PGActor/Controllers/PGPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGUI/Widget/Window/PGUIWindowRewardSelect.h"
#include "PGUI/Widget/Window/RewardSelect/PGUIRewardCard.h"
#include "PGUI/Widget/Billboard/PGUILootOverlay.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGData/PGDataTableManager.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SBox.h"
#include "AssetCompilingManager.h"
#include "PGUI/Style/PGUIStyleSettings.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundBase.h"
#include "Misc/Paths.h"
#endif

void APGPlayerController::PGRewardProbe(FString Action)
{
#if !UE_BUILD_SHIPPING
    FString TestName;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),TestName) || !TestName.StartsWith(TEXT("UI_")))
    { UE_LOG(LogTemp,Warning,TEXT("PGRewardProbe requires an isolated -PGTestProfile=UI_*")); return; }
    auto* Profile=UPGProfileSubsystem::Get(this);
    if (!Profile || !Profile->GetCatalog() || !GetPawn()) return;
    APGStageManager* Stage=nullptr;
    for (TActorIterator<APGStageManager> It(GetWorld());It;++It) { Stage=*It;break; }
    if (!Stage) return;
    auto Window=[this]() -> UPGUIWindowRewardSelect*
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UPGUIWindowRewardSelect::StaticClass(),true);
        return Widgets.IsEmpty() ? nullptr : Cast<UPGUIWindowRewardSelect>(Widgets[0]);
    };
    auto Later=[this](float Seconds,FString Next)
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,Next](float){PGRewardProbe(Next);return false;}),Seconds);
    };
    auto Check=[](bool OK,const TCHAR* Name)
    { if (!OK) UE_LOG(LogTemp,Error,TEXT("PGRewardProbe FAIL %s"),Name); };
    // Record the UI bus through the real mixer; never touch the player's profile.
    if (Action.StartsWith(TEXT("audio")))
    {
        const auto* Settings=GetDefault<UPGUIStyleSettings>();
        auto* OpenSound=Settings->RewardOpenSound.LoadSynchronous();
        auto* ConfirmSound=Settings->RewardConfirmSound.LoadSynchronous();
        auto* VictorySound=Settings->VictorySound.LoadSynchronous();
        auto* Bus=OpenSound ? Cast<USoundSubmix>(OpenSound->GetSoundSubmix()) : nullptr;
        Check(OpenSound && ConfirmSound && VictorySound && Bus,TEXT("UI sound slots and submix resolve"));
        if (!OpenSound || !ConfirmSound || !VictorySound || !Bus) return;
        Check(ConfirmSound->GetSoundSubmix()==Bus && VictorySound->GetSoundSubmix()==Bus,TEXT("shared UI recording bus"));
        auto Close=[&Window]() { if (auto* View=Window()) View->RemoveFromParent(); };
        auto Open=[this](const FString& Kind)
        {
            auto* View=CreateWidget<UPGUIWindowRewardSelect>(this,UPGUIWindowRewardSelect::StaticClass());
            if (Kind==TEXT("victory") || Kind==TEXT("paused") || Kind==TEXT("pending"))
            { FPGRunResultView Result; Result.bSavePending=Kind==TEXT("pending"); View->SetResult(Result); }
            else if (Kind==TEXT("status")) View->SetStatus(FText::FromString(TEXT("QA defeat")));
            else { View->SetChoices(FGuid::NewGuid(),{}); View->OnSubmit.BindLambda([](FGuid,int32){return false;}); }
            View->AddToViewport(100);
        };
        if (Action==TEXT("audio"))
        {
            if (GetWorld()->GetTimeSeconds()<3.f || FAssetCompilingManager::Get().GetNumRemainingAssets()>0)
            { Later(1.f,Action); return; }
            Close(); Later(.5f,TEXT("audioStart_open")); return;
        }
        FString Phase, Kind;
        if (!Action.Split(TEXT("_"),&Phase,&Kind)) return;
        if (Phase==TEXT("audioStart"))
        {
            Close();
            if (Kind==TEXT("confirm")) Open(TEXT("open"));
            Later(Kind==TEXT("confirm") ? 1.2f : .4f,TEXT("audioRecord_")+Kind); return;
        }
        if (Phase==TEXT("audioRecord"))
        {
            UAudioMixerBlueprintLibrary::StartRecordingOutput(this,4.f,Bus);
            Later(.2f,TEXT("audioPlay_")+Kind); return;
        }
        if (Phase==TEXT("audioPlay"))
        {
            if (Kind==TEXT("confirm"))
            {
                auto* View=Window();
                Check(View && !View->Cards.IsEmpty() && View->Cards[0]->GetIsEnabled(),TEXT("audio confirmation ready"));
                if (!View || View->Cards.IsEmpty() || !View->Cards[0]->GetIsEnabled()) return;
                View->BeginChoice(0); View->BeginChoice(0);
            }
            else
            {
                if (Kind==TEXT("paused")) Check(SetPause(true),TEXT("pause enabled for UI sound"));
                if (Kind==TEXT("burst")) for (int32 I=0;I<7;++I) { Open(Kind); Close(); }
                Open(Kind);
            }
            Later(Kind==TEXT("victory") || Kind==TEXT("paused") ? 2.8f : 1.2f,TEXT("audioStop_")+Kind); return;
        }
        if (Phase==TEXT("audioStop"))
        {
            const FString Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("QA")/TestName/TEXT("Audio"));
            UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,Kind,Directory,Bus);
            if (Kind==TEXT("paused")) SetPause(false);
            Close();
            UE_LOG(LogTemp,Log,TEXT("PGUISFX CAPTURE %s"),*Kind);
            const TArray<FString> Cases={TEXT("open"),TEXT("confirm"),TEXT("victory"),TEXT("pending"),TEXT("status"),TEXT("burst"),TEXT("paused")};
            const int32 Index=Cases.Find(Kind);
            if (Cases.IsValidIndex(Index+1)) Later(.5f,TEXT("audioStart_")+Cases[Index+1]);
            else UE_LOG(LogTemp,Log,TEXT("PGUISFX CAPTURES COMPLETE"));
            return;
        }
        return;
    }
    if ((Action.StartsWith(TEXT("check")) || Action==TEXT("capture")) && FAssetCompilingManager::Get().GetNumRemainingAssets()>0)
    { Later(.5f,Action); return; }
    if (Action==TEXT("capture"))
    {
        FScreenshotRequest::RequestScreenshot(TEXT("PGReward"),true,true);
        return;
    }
    if (Action.StartsWith(TEXT("check")))
    {
        if (Action==TEXT("checkLoot") || Action==TEXT("checkLootFar"))
        {
            Check(LootOverlay && LootOverlay->ValidateLayoutForQA(),TEXT("loot packing and target"));
            auto* Nearest=APGLootDrop::FindNearestPickup(GetPawn());
            Check((Nearest!=nullptr)==(Action==TEXT("checkLoot")),TEXT("near and distant pickup target eligibility"));
            if (Nearest)
            {
                const auto Guid=Nearest->GetItem().Guid;
                Profile->bInjectSaveFailure=true; PickupNearest(); Profile->bInjectSaveFailure=false;
                Check(IsValid(Nearest) && !Profile->HasClaimedLoot(Guid),TEXT("failed pickup retains drop"));
                PickupNearest();
                Check(Profile->HasClaimedLoot(Guid),TEXT("highlighted pickup retry commits"));
            }
            UE_LOG(LogTemp,Log,TEXT("PGRewardProbe CHECK loot complete")); Later(.3f,TEXT("capture")); return;
        }
        auto* View=Window();
        if (!View) { Check(false,TEXT("window exists")); return; }
        // A startup render hitch can advance the core ticker before Slate has animated a frame.
        if (View->PresentationTime<.6f) { Later(.5f,Action); return; }
        Check(IsMoveInputIgnored() && !IsPaused(),TEXT("stage modal owns input without pausing"));
        const auto Size=View->GetCachedGeometry().GetLocalSize();
        const auto FrameSize=View->Frame->GetCachedGeometry().GetLocalSize();
        Check(FrameSize.X<=Size.X && FrameSize.Y<=Size.Y,TEXT("responsive frame fits viewport"));
        Check(View->FrameTexture!=nullptr,TEXT("generated trial frame is loaded"));
        if (View->bIsStatus && !View->bIsResult)
        {
            Check(FrameSize.X<=900 && FrameSize.Y<=570,TEXT("failure uses compact result layout"));
            Check(!View->StatusText.ToString().Contains(TEXT("Player defeated")) && !View->StatusText.ToString().Contains(TEXT("Missing enemy")),TEXT("diagnostic English is not displayed"));
        }
        for (const auto& Card : View->Cards)
            Check(Card->GetCachedGeometry().GetLocalSize().Y>250 && Card->GetIsEnabled(),TEXT("cards arranged and reveal finished"));
        const FModifierKeysState Modifiers;
        FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,Modifiers,0,false,0,0));
        FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Escape,Modifiers,0,false,0,0));
        Check(View->IsInViewport() && IsMoveInputIgnored(),TEXT("Escape preserves pending choice or result"));
        if (Action==TEXT("checkInteraction"))
        {
            Check(!View->SubmitChoice(-2) && !View->SubmitChoice(99),TEXT("invalid input rejected"));
            Profile->bInjectSaveFailure=true; View->BeginChoice(0); View->BeginChoice(1);
            Check(View->PendingIndex==0,TEXT("confirmation ignores second choice"));
            Later(.5f,TEXT("checkFailure")); return;
        }
        if (Action==TEXT("checkFailure"))
        {
            Check(View->Token.IsValid() && !View->Feedback.IsEmpty() && !View->bConfirming,TEXT("failure feedback and retry retained"));
            Profile->bInjectSaveFailure=false;
            View->BeginChoice(0); Later(.5f,TEXT("verifyCommitted")); return;
        }
        if (Action==TEXT("checkPending"))
        {
            Check(View->Result.bSavePending && !Profile->GetProfile()->bRunEnded,TEXT("pending result does not claim saved victory"));
            const FGuid Guid=View->Result.Loot.Guid;
            Profile->bInjectSaveFailure=false; View->RetryRun(); View->RetryRun();
            Check(View->bConfirming && Guid.IsValid(),TEXT("retry is debounced")); Later(.7f,TEXT("verifyResult")); return;
        }
        UE_LOG(LogTemp,Log,TEXT("PGRewardProbe CHECK layout complete")); Later(.2f,TEXT("capture")); return;
    }
    if (Action==TEXT("verifyCommitted"))
    {
        Check(!Window() && !IsMoveInputIgnored(),TEXT("committed selection releases modal"));
        Check(Profile->GetProfile()->SelectedRewards.FindRef(15018)==1,TEXT("reward committed exactly once"));
        UE_LOG(LogTemp,Log,TEXT("PGRewardProbe CHECK interaction complete"));
        PGRewardProbe(TEXT("reward")); return;
    }
    if (Action==TEXT("verifyResult"))
    {
        const auto* View=Window();
        Check(View && !View->Result.bSavePending && Profile->GetProfile()->bRunEnded,TEXT("retry transitions to saved result"));
        Check(Profile->GetProfile()->CompletedRuns==1 && View && View->Result.Loot.Guid==Profile->GetProfile()->BossReward.Guid,TEXT("same loot and one victory"));
        Profile->bInjectSaveFailure=true;
        if (auto* Mutable=Window()) Mutable->RetryRun();
        Later(.5f,TEXT("verifyRestartFailure")); return;
    }
    if (Action==TEXT("verifyRestartFailure"))
    {
        const auto* View=Window();
        Check(View && !View->Feedback.IsEmpty() && !View->bConfirming && Profile->GetProfile()->BossReward.Guid.IsValid(),TEXT("failed new run retains result and enables retry"));
        Profile->bInjectSaveFailure=false;
        UE_LOG(LogTemp,Log,TEXT("PGRewardProbe CHECK pending complete")); Later(.2f,TEXT("capture")); return;
    }
    Profile->bInjectSaveFailure=false;
    if (Action.StartsWith(TEXT("loot")))
    {
        // Let the pawn land before anchoring drops; the spawn transform may be above the floor.
        if (GetWorld()->GetTimeSeconds()<2.f) { Later(2.f,Action); return; }
        FPGStagePresentation Close; Close.Owner=Stage; Close.bClose=true;
        UPGMessageManager::Get(this)->SendMessage(EPGUIMessageType::StagePresentation,&Close);
        const int32 Count=Action==TEXT("loot100") ? 100 : 12;
        const auto* Catalog=Profile->GetCatalog();
        if (Catalog->Items.IsEmpty()) return;
        for (int32 I=0;I<Count;++I)
        {
            const auto& Def=Catalog->Items[I%Catalog->Items.Num()];
            FPGItemInstance Item; Item.Guid=FGuid(0,0,0,I+1); Item.DefinitionId=Def.Id; Item.Options=Def.BaseOptions;
            const FVector Offset=Action==TEXT("lootFar") ? FVector(700+I*4,0,0) : FVector((I%4)*12,80+(I/4)*8,-65);
            auto* Drop=GetWorld()->SpawnActor<APGLootDrop>(GetPawn()->GetActorLocation()+Offset,FRotator::ZeroRotator);
            Drop->InitializeItem(Item);
        }
        Later(3.f,Action==TEXT("lootFar") ? TEXT("checkLootFar") : TEXT("checkLoot"));
    }
    else if (Action==TEXT("defeat") || Action==TEXT("interrupted"))
    {
        Check(Profile->ConfigureBuildScenario({15000,15003}),TEXT("valid failed run fixture"));
        Stage->CurrentStageId=3;
        Stage->FailStage(Action==TEXT("defeat") ? TEXT("Player defeated.") : TEXT("Missing enemy class: 99999"));
        Later(3.f,TEXT("checkLayout"));
    }
    else if (Action==TEXT("result") || Action==TEXT("pending") || Action==TEXT("pendingRetry"))
    {
        Check(Profile->ConfigureBuildScenario({15000,15001,15003,15018}),TEXT("valid result build fixture"));
        Check(Profile->CommitReward(FGuid::NewGuid(),2,EPGStatType::Attack,55,EPGCombatPerk::None,0,15014),TEXT("stat reward fixture"));
        Check(Profile->CommitReward(FGuid::NewGuid(),2,EPGStatType::Attack,55,EPGCombatPerk::None,0,15014),TEXT("repeated reward counted separately"));
        Check(Profile->CommitReward(FGuid::NewGuid(),2,EPGStatType::CriticalRate,500,EPGCombatPerk::None,0,15017),TEXT("critical reward fixture"));
        const auto* Catalog=Profile->GetCatalog();
        FRandomStream Random(73); FPGItemInstance Item;
        const auto* Pool=Catalog->DropPools.FindByPredicate([](const auto& Entry){return Entry.Id.ToString().Contains(TEXT("Boss"));});
        Check(Pool && Profile->RollDrop(Random,Item,Pool->Id),TEXT("boss loot fixture"));
        Item.Guid=FGuid::NewGuid(); Check(Profile->QueueBossReward(Item),TEXT("queue exact boss reward"));
        Profile->bInjectSaveFailure=Action!=TEXT("result");
        Stage->CurrentStageId=6; Stage->CurrentStageState=EPGStageState::Completed; Stage->GoToNextStage();
        Later(3.f,Action==TEXT("pendingRetry") ? TEXT("checkPending") : TEXT("checkLayout"));
    }
    else
    {
        Check(Profile->ConfigureBuildScenario({15000,15003}),TEXT("valid choice build fixture"));
        FPGStagePresentation View; View.Owner=Stage; View.Token=FGuid::NewGuid();
        if (Action!=TEXT("empty"))
            for (int32 Id : {15018,15004,15014}) { FPGStageReward Reward; Reward.RewardId=Id; Reward.RewardType=EPGRewardType::Stat; View.Choices.Add(Reward); }
        const auto Choices=View.Choices;
        View.Submit.BindLambda([Profile,Stage,Choices](FGuid Token,int32 Index)
        {
            const auto* Reward=Choices.IsValidIndex(Index) ? PGData()->GetRowData<FPGRewardStatDataRow>(Choices[Index].RewardId) : nullptr;
            if (!Profile->CommitReward(Token,2,Reward ? Reward->StatType : EPGStatType::None,Reward ? Reward->Amount : 0,
                Reward ? Reward->Perk : EPGCombatPerk::None,Reward ? Reward->PerkPercent : 0,Reward ? Reward->StatId : 0)) return false;
            FPGStagePresentation Close; Close.Owner=Stage; Close.bClose=true;
            UPGMessageManager::Get(Stage)->SendMessage(EPGUIMessageType::StagePresentation,&Close); return true;
        });
        UPGMessageManager::Get(this)->SendMessage(EPGUIMessageType::StagePresentation,&View);
        Later(3.f,Action==TEXT("interaction") ? TEXT("checkInteraction") : TEXT("checkLayout"));
    }
    UE_LOG(LogTemp,Log,TEXT("PGRewardProbe OPEN mode=%s"),*Action);
#endif
}

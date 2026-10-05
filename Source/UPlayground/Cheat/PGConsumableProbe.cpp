#include "PGCheatManager.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGConsumableComponent.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGUI/Widget/HUD/PGUIMainHUD.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Layout/Children.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
TSharedPtr<SWidget> FindPotionWidget(const TSharedRef<SWidget>& Root,FName Tag)
{
    if(Root->GetTag()==Tag) return Root;
    if(auto* Children=Root->GetChildren())
        for(int32 I=0;I<Children->Num();++I)
            if(auto Found=FindPotionWidget(Children->GetChildAt(I),Tag)) return Found;
    return nullptr;
}
}

void UPGCheatManager::PGPotion(int32 Charges, float HealthFraction)
{
#if !UE_BUILD_SHIPPING
    auto* P=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* Profile=UPGProfileSubsystem::Get(this);
    if(!P || !FMath::IsFinite(HealthFraction) || !Profile || !Profile->MarkRunAssisted()) return;
    auto* ASC=P->GetPGAbilitySystemComponent();
    if(ASC->GetHealth()<=0) return;
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),ASC->GetCombatStat(EPGStatType::Health)*FMath::Clamp(HealthFraction,.01f,1.f));
    P->GetConsumableComponent()->DebugSetCharges(Charges);
#endif
}

void UPGCheatManager::PGConsumableProbe()
{
#if !UE_BUILD_SHIPPING
    FString ProfileName;
    if(!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),ProfileName) || !ProfileName.StartsWith(TEXT("Consumables_"))) return;
    struct FState
    {
        double Start=FPlatformTime::Seconds(), PauseAt=0.;
        float At=0.f, MaxHealth=0.f, PausedCooldown=0.f;
        int32 Phase=0;
        int32 ClickStep=0;
        TWeakPtr<SWidget> Layout, Button;
        FVector2D LayoutSize, LayoutPosition, ButtonSize;
        bool bShot=false;
    };
    auto S=MakeShared<FState>(); TWeakObjectPtr<UWorld> Weak=GetWorld();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak](float)
    {
        auto* W=Weak.Get(); if(!W) return false;
        auto* P=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        auto* PC=P ? Cast<APGPlayerController>(P->GetController()) : nullptr;
        auto Finish=[W](bool Pass,const TCHAR* Reason)
        {
            UE_LOG(LogTemp,Display,TEXT("PGConsumableProbe %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),Reason);
            UGameplayStatics::SetGamePaused(W,false);
            if(auto* Controller=W->GetFirstPlayerController()) Controller->ConsoleCommand(TEXT("quit"));
            return false;
        };
        if(FPlatformTime::Seconds()-S->Start>100.) return Finish(false,TEXT("timeout"));
        if(!P || !PC || FPlatformTime::Seconds()-S->Start<5.) return true;
        APGStageManager* Stage=nullptr; for(TActorIterator<APGStageManager> It(W);It;++It){Stage=*It;break;}
        if(!Stage) return Finish(false,TEXT("missing stage"));
        auto* Potion=P->GetConsumableComponent(); auto* ASC=P->GetPGAbilitySystemComponent();
        auto* Profile=UPGProfileSubsystem::Get(W);
        const float Age=W->GetTimeSeconds()-S->At;
        if(S->Phase>0)
        {
            auto Layout=S->Layout.Pin(), Button=S->Button.Pin();
            if(!Layout || !Button || !FVector2D(Layout->GetCachedGeometry().GetAbsoluteSize()).Equals(S->LayoutSize,.1) ||
                !FVector2D(Layout->GetCachedGeometry().GetAbsolutePosition()).Equals(S->LayoutPosition,.1) ||
                !FVector2D(Button->GetCachedGeometry().GetAbsoluteSize()).Equals(S->ButtonSize,.1))
                return Finish(false,TEXT("HUD resized or moved after button activation/status change"));
        }
        auto Next=[&](int32 Phase){S->Phase=Phase;S->At=W->GetTimeSeconds();S->bShot=false;};
        auto Key=[PC](bool Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Q,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
        auto Health=[&](float Fraction){ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),S->MaxHealth*Fraction);};
        auto Capture=[&](const TCHAR* Name)
        {
            if(S->bShot || Age<.3f) return;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("QA/Consumables")/(FString(Name)+TEXT(".png")),true,false);
            S->bShot=true;
        };
        if(S->Phase==0)
        {
            if(!Potion->GetDefinition() || Potion->GetState().Count!=3) return Finish(false,TEXT("preparation supply"));
            for(TObjectIterator<UPGUIMainHUD> It;It;++It)
                if(It->GetWorld()==W && It->GetOwningPlayer()==PC)
                {
                    auto Root=It->TakeWidget();
                    S->Layout=FindPotionWidget(Root,TEXT("PGCombatHUDLayout"));
                    S->Button=FindPotionWidget(Root,TEXT("PGHealingPotion"));
                    break;
                }
            if(!S->Layout.IsValid() || !S->Button.IsValid()) return Finish(false,TEXT("missing HUD widgets"));
            S->LayoutSize=S->Layout.Pin()->GetCachedGeometry().GetAbsoluteSize();
            S->LayoutPosition=S->Layout.Pin()->GetCachedGeometry().GetAbsolutePosition();
            S->ButtonSize=S->Button.Pin()->GetCachedGeometry().GetAbsoluteSize();
            UE_LOG(LogTemp,Display,TEXT("PGConsumableProbe HUD before-click layout=%s button=%s"),*S->LayoutSize.ToString(),*S->ButtonSize.ToString());
            S->MaxHealth=ASC->GetCombatStat(EPGStatType::Health);
            Next(1); return true;
        }
        if(S->Phase==1)
        {
            auto Button=StaticCastSharedPtr<SButton>(S->Button.Pin());
            const FKeyEvent Enter(EKeys::Enter,FModifierKeysState(),0,false,0,0);
            if(S->ClickStep==0 && Age>.12f) { Button->OnKeyDown(Button->GetCachedGeometry(),Enter); S->ClickStep=1; }
            if(S->ClickStep==1 && Age>.25f)
            {
                Button->OnKeyUp(Button->GetCachedGeometry(),Enter); S->ClickStep=2;
                if(Potion->GetState().Count!=3 || Potion->GetState().Notice.IsEmpty()) return Finish(false,TEXT("HUD button callback"));
            }
            Capture(TEXT("ready")); if(Age<.7f || FScreenshotRequest::IsScreenshotRequested()) return true;
            Stage->ReadyForNextStage();
            W->GetTimerManager().ClearAllTimersForObject(Stage);
            Stage->CurrentStageState=EPGStageState::InProgress;
            Health(.25f); Next(2); return true;
        }
        if(S->Phase==2)
        {
            Capture(TEXT("low_health")); if(Age<.7f || FScreenshotRequest::IsScreenshotRequested()) return true;
            Key(true); Next(3); return true;
        }
        if(S->Phase==3)
        {
            if(Age<.2f) return true;
            if(Potion->GetState().Count!=2 || !FMath::IsNearlyEqual(ASC->GetHealth(),S->MaxHealth*.65f,.01f))
                return Finish(false,TEXT("Q press did not heal once"));
            Capture(TEXT("healed"));
            if(Age<8.3f || FScreenshotRequest::IsScreenshotRequested()) return true;
            if(Potion->GetState().Count!=2) return Finish(false,TEXT("held Q repeated after cooldown"));
            Key(false); Next(4); return true;
        }
        if(S->Phase==4 && Age>.15f)
        {
            P->SetIsCanControl(false); Health(.25f); Key(true); Next(5); return true;
        }
        if(S->Phase==5 && Age>.2f)
        {
            Key(false);
            if(Potion->GetState().Count!=1 || P->GetIsCacControl() || !FMath::IsNearlyEqual(ASC->GetHealth(),S->MaxHealth*.65f,.01f))
                return Finish(false,TEXT("hit lock healing"));
            P->SetIsCanControl(true);
            const float Cooldown=Potion->GetRemainingCooldown();
            if(!Profile->MarkRunAssisted() || Potion->GetState().Count!=1 || !FMath::IsNearlyEqual(Cooldown,Potion->GetRemainingCooldown(),.01f))
                return Finish(false,TEXT("profile commit reset supply"));
            PC->ToggleInventory();
            if(!W->IsPaused() || !UPGUIManager::Get(W)->IsWindowOpen() || Potion->TryUse()) return Finish(false,TEXT("inventory guard"));
            S->PausedCooldown=Potion->GetRemainingCooldown(); S->PauseAt=FPlatformTime::Seconds(); Next(6); return true;
        }
        if(S->Phase==6)
        {
            if(FPlatformTime::Seconds()-S->PauseAt<.4) return true;
            if(!FMath::IsNearlyEqual(S->PausedCooldown,Potion->GetRemainingCooldown(),.001f)) return Finish(false,TEXT("cooldown advanced during pause"));
            PC->CloseInventory(); Next(7); return true;
        }
        if(S->Phase==7 && Potion->GetRemainingCooldown()<=0 && Age>.2f)
        {
            Health(.25f);
            // Use during a real attack, preserving its logical clock and hit phases.
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
            if(!P->GetPlayerAttackComponent()->IsRunning()) return Finish(false,TEXT("attack fixture"));
            Key(true); Next(8); return true;
        }
        if(S->Phase==8 && Age>.15f)
        {
            Key(false);
            if(Potion->GetState().Count!=0 || !P->GetPlayerAttackComponent()->IsRunning()) return Finish(false,TEXT("potion interrupted attack"));
            if(Potion->TryUse()) return Finish(false,TEXT("empty potion use"));
            Next(9); return true;
        }
        if(S->Phase==9)
        {
            Capture(TEXT("empty")); if(Age<1.6f || FScreenshotRequest::IsScreenshotRequested()) return true;
            ASC->CancelAbilities(); ASC->ClearBufferedInput();
            const float Before=ASC->GetHealth(), Cooldown=Potion->GetRemainingCooldown();
            Stage->PrepareWave(1); W->GetTimerManager().ClearAllTimersForObject(Stage);
            if(Potion->GetState().Count!=0 || ASC->GetHealth()!=Before || Potion->GetRemainingCooldown()!=Cooldown)
                return Finish(false,TEXT("wave transition refilled"));
            Stage->CurrentStageState=EPGStageState::InProgress;
            Stage->CurrentWaveIndex=Stage->ActiveWaves.Num()-1;
            Stage->RemainingMonsters=0; Stage->MonsterSpawnQueue.Reset();
            Stage->CheckStageComplete();
            if(Stage->GetCurrentStageState()!=EPGStageState::BuildPhase || Potion->GetState().Count!=3 || ASC->GetHealth()!=S->MaxHealth || Potion->GetRemainingCooldown()!=0)
                return Finish(false,TEXT("clear did not restock"));
            if(!Stage->CommitReward(Stage->RewardToken,0)) return Finish(false,TEXT("reward commit"));
            Stage->ReadyForNextStage(); W->GetTimerManager().ClearAllTimersForObject(Stage);
            S->MaxHealth=ASC->GetCombatStat(EPGStatType::Health);
            if(Stage->GetCurrentStageId()!=2 || Potion->GetState().Count!=3 || ASC->GetHealth()!=S->MaxHealth)
                return Finish(false,TEXT("next stage supply"));
            Stage->CurrentStageState=EPGStageState::InProgress;
            Health(.25f);
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
            if(!P->GetPlayerDashComponent()->IsDashing()) return Finish(false,TEXT("dash fixture"));
            Key(true); Next(10); return true;
        }
        if(S->Phase==10 && Age>.1f)
        {
            Key(false);
            if(Potion->GetState().Count!=2 || !P->GetPlayerDashComponent()->IsDashing()) return Finish(false,TEXT("potion interrupted dash"));
            Next(11); return true;
        }
        if(S->Phase==11 && Age>.8f)
        {
            Health(0);
            if(Potion->TryUse() || Potion->GetState().Count!=2 || ASC->GetHealth()!=0) return Finish(false,TEXT("death use or charge loss"));
            return Finish(true,TEXT("Q held input hit-lock attack dash pause profile wave clear next-stage death HUD stable-click-geometry"));
        }
        return true;
    }));
#endif
}

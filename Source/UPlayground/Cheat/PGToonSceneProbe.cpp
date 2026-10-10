#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"

// Bounded, isolated renderer workload: real forest stage/AI/GAS, scripted input.
// Health is restored during the soak; this is not a balance or human-play test.
static FAutoConsoleCommandWithWorld PGToonSceneProbe(
    TEXT("PGToonSceneProbe"), TEXT("Capture a 60s forest combat CSV in a ToonScene_ test profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile) ||
        !Profile.StartsWith(TEXT("ToonScene_"))) return;
    struct FState
    {
        double Start = FPlatformTime::Seconds(), CapturedAt = 0., StoppedAt = 0., InputAt = 0., RestartAt = 0.;
        int32 Phase = 0, Inputs = 0, ActiveAttacks = 0, MaxEnemies = 0;
    };
    auto S = MakeShared<FState>();
    TWeakObjectPtr<UWorld> WeakWorld = World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S, WeakWorld](float)
    {
        auto* W = WeakWorld.Get();
        if (!W) return false;
        const double Now = FPlatformTime::Seconds();
        auto Finish = [&](bool bOK)
        {
            UE_LOG(LogTemp, Display, TEXT("PGToonScene %s inputs=%d active=%d enemies=%d scripted=1 health_restored=1"),
                bOK ? TEXT("PASS") : TEXT("FAIL"), S->Inputs, S->ActiveAttacks, S->MaxEnemies);
            FPlatformMisc::RequestExit(false);
            return false;
        };
        if (Now - S->Start > 150.) return Finish(false);
        auto* P = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W, 0));
        if (!P || !P->GetPGAbilitySystemComponent() || Now - S->Start < 15.) return true;
        auto* ASC = P->GetPGAbilitySystemComponent();
        ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),
            ASC->GetNumericAttribute(UPGAtrributeSet::GetMaxHealthAttribute()));
        if (S->Phase == 0)
        {
            auto* Profile = UPGProfileSubsystem::Get(W);
            if (!Profile || !Profile->SelectCharacter(TEXT("Bokusei"))) return Finish(false);
            P->SetCameraMode(FParse::Param(FCommandLine::Get(), TEXT("PGToonActionCamera")) ? EPGCameraMode::Action3D : EPGCameraMode::QuarterView);
            for (TActorIterator<APGStageManager> It(W); It; ++It) { It->StartStage(1); break; }
            GEngine->Exec(W, TEXT("DisableAllScreenMessages"));
            GEngine->Exec(W, TEXT("t.MaxFPS 0"));
            GEngine->Exec(W, TEXT("r.VSync 0"));
            S->Phase = 1;
        }
        APGCharacterEnemy* Nearest = nullptr;
        double NearestSq = TNumericLimits<double>::Max();
        int32 Count = 0;
        for (TActorIterator<APGCharacterEnemy> It(W); It; ++It)
        {
            if (It->IsActorBeingDestroyed() || It->IsHidden()) continue;
            ++Count;
            const double Sq = FVector::DistSquared(P->GetActorLocation(), It->GetActorLocation());
            if (Sq < NearestSq) { NearestSq = Sq; Nearest = *It; }
        }
        S->MaxEnemies = FMath::Max(S->MaxEnemies, Count);
        if (Count == 0 && Now - S->RestartAt > 12.)
        {
            for (TActorIterator<APGStageManager> It(W); It; ++It)
                if (It->GetCurrentStageState() == EPGStageState::RewardPhase || It->GetCurrentStageState() == EPGStageState::Finished)
                { It->StartStage(1); S->RestartAt = Now; break; }
        }
        if (Nearest && !P->GetPlayerAttackComponent()->IsRunning())
        {
            const FVector Direction = (Nearest->GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
            P->SetActorRotation(Direction.Rotation());
            if (NearestSq > FMath::Square(200.)) P->AddMovementInput(Direction);
        }
        if (Now - S->InputAt > 1.1)
        {
            const FGameplayTag Tag = S->Inputs % 4 == 0 ? PGGamePlayTags::InputTag_Skill_Slot1 : PGGamePlayTags::InputTag_Skill_Normal;
            ASC->OnAbilityInputPressed(Tag);
            ASC->OnAbilityInputReleased(Tag);
            ++S->Inputs; S->InputAt = Now;
        }
        if (P->GetPlayerAttackComponent()->IsRunning()) ++S->ActiveAttacks;
        if (S->Phase == 1 && Now - S->Start > 35.)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("ToonScene.png"), false, false);
            S->CapturedAt = Now; S->Phase = 2;
        }
        else if (S->Phase == 2 && Now - S->CapturedAt > 3.)
        {
            GEngine->Exec(W, TEXT("CsvProfile STARTFILE=ToonScene.csv"));
            GEngine->Exec(W, TEXT("CsvProfile START"));
            S->CapturedAt = Now; S->Phase = 3;
        }
        else if (S->Phase == 3 && Now - S->CapturedAt >= 60.)
        {
            GEngine->Exec(W, TEXT("csvprofile stop"));
            S->StoppedAt = Now; S->Phase = 4;
        }
        else if (S->Phase == 4 && Now - S->StoppedAt > 5.)
            return Finish(S->Inputs > 30 && S->ActiveAttacks > 120 && S->MaxEnemies >= 3);
        return true;
    }));
}));
#endif

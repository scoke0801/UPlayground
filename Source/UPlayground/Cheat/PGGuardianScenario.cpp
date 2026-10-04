#include "PGCheatManager.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGAI/PGRoleAIController.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGUI/Widget/HUD/PGUIMainHUD.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/OverlapResult.h"
#endif

void UPGCheatManager::PGGuardianScenario(int32 PackCount)
{
#if !UE_BUILD_SHIPPING
    FString TestProfile;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), TestProfile)
        || !TestProfile.StartsWith(TEXT("GuardianSoak_"))) return;
    APawn* Player = GetOuterAPlayerController()->GetPawn();
    auto* Profile = UPGProfileSubsystem::Get(this);
    auto* Tables = UPGDataTableManager::Get(this);
    if (!Player || !Profile || !Tables || !Profile->MarkRunAssisted()) return;

    TArray<APGCharacterEnemy*> Previous;
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("PGGuardianSoak"))) Previous.Add(*It);
    for (auto* Enemy : Previous)
    {
        auto* Controller = Enemy->GetController();
        if (auto* AI = Cast<APGRoleAIController>(Controller)) AI->SetCombatThinkingEnabled(false);
        Enemy->Destroy();
        if (Controller) Controller->Destroy();
    }

    PackCount = FMath::Clamp(PackCount, 0, 10);
    int32 Spawned = 0;
    for (int32 Pack = 0; Pack < PackCount; ++Pack)
        for (const int32 TID : {15103, 15103, 15102, 15102, 15102})
        {
            const auto* Row = Tables->GetRowData<FPGEnemyDataRow>(TID);
            UClass* Class = Row ? Row->ActorClass.LoadSynchronous() : nullptr;
            if (!Class) continue;
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy = GetWorld()->SpawnActor<APGCharacterEnemy>(Class,
                Player->GetActorLocation() + FVector(800.f + Spawned * 90.f, 800.f, 0.f), FRotator::ZeroRotator, Params);
            if (!Enemy) continue;
            Enemy->Tags.Add(TEXT("PGGuardianSoak"));
            Enemy->Tags.Add(FName(*FString::Printf(TEXT("PGGuardianRole_%d"), TID)));
            if (Spawned == 0) Enemy->Tags.Add(TEXT("PGGuardianPrimary"));
            // This fixture has no stage spawn identity and must never award loot.
            Enemy->bCanDropLoot = false;
            ++Spawned;
        }
    UE_LOG(LogTemp, Display, TEXT("PGGuardianScenario packs=%d spawned=%d assisted=1"), PackCount, Spawned);
#endif
}

void UPGCheatManager::PGGuardianBenchmark(int32 Seconds)
{
#if !UE_BUILD_SHIPPING
    FString TestProfile, RunID;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), TestProfile)
        || !TestProfile.StartsWith(TEXT("GuardianSoak_"))
        || !FParse::Value(FCommandLine::Get(), TEXT("PGGuardianBenchmarkId="), RunID)
        || RunID.IsEmpty() || RunID.Contains(TEXT("/")) || RunID.Contains(TEXT("\\"))) return;
    if (GetWorld()->GetTimerManager().IsTimerActive(GuardianBenchmarkTimer)) return;
    Seconds = FMath::Clamp(Seconds, 120, 3600);
    int32 FrameCap = 60;
    FParse::Value(FCommandLine::Get(), TEXT("PGGuardianBenchmarkFPS="), FrameCap);
    FrameCap = FMath::Clamp(FrameCap, 0, 240);
    struct FState
    {
        int32 Phase = -1, Windups = 0, Recoveries = 0, WeaponsBefore = 0;
        double StartedAt = 0, GameStartedAt = 0, NextReset = 0;
        bool bCapturing = false;
        TArray<TWeakObjectPtr<APGCharacterEnemy>> Enemies;
        TMap<TWeakObjectPtr<APGCharacterEnemy>, uint8> Previous;
    };
    const auto State = MakeShared<FState>();
    GetWorld()->GetTimerManager().SetTimer(GuardianBenchmarkTimer, FTimerDelegate::CreateWeakLambda(this, [this, State, Seconds, RunID, FrameCap]()
    {
        auto* PC = GetOuterAPlayerController();
        APawn* Player = PC ? PC->GetPawn() : nullptr;
        if (!Player) return;
        const double Now = FPlatformTime::Seconds();
        const TCHAR* Names[] = {TEXT("baseline"), TEXT("dense"), TEXT("return")};
        const int32 Packs[] = {1, 10, 1};
        const int32 Durations[] = {Seconds / 4, Seconds / 2, Seconds - Seconds / 4 - Seconds / 2};
        const auto Arrange = [&]()
        {
            int32 GuardianIndex = 0, ShooterIndex = 0;
            for (const auto& Weak : State->Enemies) if (auto* Enemy = Weak.Get())
            {
                auto* AI = Cast<APGRoleAIController>(Enemy->GetController());
                const bool bGuardian = Enemy->ActorHasTag(TEXT("PGGuardianRole_15103"));
                const int32 Index = bGuardian ? GuardianIndex++ : ShooterIndex++;
                const int32 Count = Packs[State->Phase] * (bGuardian ? 2 : 3);
                const float Angle = 2.f * PI * Index / FMath::Min(Count, 10);
                const float Radius = (bGuardian ? 210.f : 550.f) + (Index / 10) * 110.f;
                const FVector Origin = Player->GetActorLocation();
                const FVector Point = Origin + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
                if (AI) AI->SetCombatThinkingEnabled(false);
                Enemy->SetActorLocationAndRotation(Point, (Origin - Point).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
                if (AI) AI->SetCombatThinkingEnabled(true);
            }
            PGStress(0, 0);
            State->NextReset = Now + 60;
        };
        if (State->Phase < 0 || Now - State->StartedAt >= Durations[State->Phase])
        {
            if (State->Phase >= 0)
            {
                PC->ConsoleCommand(TEXT("CsvProfile STOP"));
                const double GameSeconds = GetWorld()->GetTimeSeconds() - State->GameStartedAt;
                UE_LOG(LogTemp, Display, TEXT("PGGuardianBenchmark phase=%s duration=%.3f game_seconds=%.3f windups=%d recoveries=%d"),
                    Names[State->Phase], Now - State->StartedAt, GameSeconds, State->Windups, State->Recoveries);
                if (GameSeconds < Durations[State->Phase] * .9 || State->Windups == 0 || State->Recoveries == 0)
                {
                    PGGuardianScenario(0);
                    UE_LOG(LogTemp, Error, TEXT("PGGuardianBenchmark FAILED clock/activity"));
                    GetWorld()->GetTimerManager().ClearTimer(GuardianBenchmarkTimer);
                    return;
                }
            }
            else
            {
                PC->ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), FrameCap));
                UE_LOG(LogTemp, Display, TEXT("PGGuardianBenchmark fps_cap=%d assisted=1 direct_input=0"), FrameCap);
                PC->ConsoleCommand(TEXT("r.VSync 0"));
                PC->ConsoleCommand(TEXT("t.IdleWhenNotForeground 0"));
                TArray<UUserWidget*> Widgets;
                UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), false);
                for (auto* Widget : Widgets) if (Widget->IsInViewport() && !Widget->IsA<UPGUIMainHUD>()) Widget->RemoveFromParent();
                UGameplayStatics::SetGamePaused(this, false);
                for (TActorIterator<APGWeaponBase> It(GetWorld()); It; ++It) ++State->WeaponsBefore;
            }
            if (++State->Phase == 3)
            {
                PGGuardianScenario(0);
                int32 Remaining = 0, Weapons = 0;
                for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("PGGuardianSoak"))) ++Remaining;
                for (TActorIterator<APGWeaponBase> It(GetWorld()); It; ++It) ++Weapons;
                if (Remaining || Weapons != State->WeaponsBefore)
                {
                    UE_LOG(LogTemp, Error, TEXT("PGGuardianBenchmark FAILED cleanup"));
                }
                else
                {
                    UE_LOG(LogTemp, Display, TEXT("PGGuardianBenchmark COMPLETE enemies=0 weapons=%d assisted=1 direct_input=0"), Weapons);
                }
                // Clearing the currently executing weak lambda destroys its captures.
                // This must be the final operation before returning.
                GetWorld()->GetTimerManager().ClearTimer(GuardianBenchmarkTimer);
                return;
            }
            PGGuardianScenario(Packs[State->Phase]);
            State->Enemies.Reset(); State->Previous.Reset();
            for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("PGGuardianSoak"))) State->Enemies.Add(*It);
            if (State->Enemies.Num() != Packs[State->Phase] * 5)
            {
                PGGuardianScenario(0);
                UE_LOG(LogTemp, Error, TEXT("PGGuardianBenchmark FAILED spawn count"));
                GetWorld()->GetTimerManager().ClearTimer(GuardianBenchmarkTimer);
                return;
            }
            for (const auto& Weak : State->Enemies) if (const auto* Enemy = Weak.Get())
            {
                TArray<FOverlapResult> Hits;
                GetWorld()->OverlapMultiByObjectType(Hits, Enemy->GetActorLocation(), FQuat::Identity,
                    FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeSphere(20.f));
                const bool bCapsuleHit = Hits.ContainsByPredicate([Enemy](const FOverlapResult& Hit)
                {
                    return Hit.GetActor() == Enemy && Hit.GetComponent() == Enemy->GetCapsuleComponent();
                });
                if (Enemy->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision ||
                    Enemy->GetMesh()->GetCollisionEnabled() != ECollisionEnabled::NoCollision || !bCapsuleHit)
                {
                    PGGuardianScenario(0);
                    UE_LOG(LogTemp, Error, TEXT("PGGuardianBenchmark FAILED collision policy"));
                    GetWorld()->GetTimerManager().ClearTimer(GuardianBenchmarkTimer);
                    return;
                }
            }
            State->StartedAt = Now; State->GameStartedAt = GetWorld()->GetTimeSeconds();
            State->Windups = State->Recoveries = 0; State->bCapturing = false;
            Arrange();
        }
        if (!State->bCapturing && Now - State->StartedAt >= (Seconds >= 1200 ? 30 : 5))
        {
            PC->ConsoleCommand(FString::Printf(TEXT("CsvProfile STARTFILE=%s_%s.csv"), *RunID, Names[State->Phase]));
            PC->ConsoleCommand(TEXT("CsvProfile START"));
            State->bCapturing = true;
        }
        if (Now >= State->NextReset) Arrange();
        for (const auto& Weak : State->Enemies) if (const auto* Enemy = Weak.Get())
        {
            if (!Enemy->ActorHasTag(TEXT("PGGuardianRole_15103"))) continue;
            const uint8 Current = uint8(Enemy->bPatternActive) | (uint8(Enemy->bPatternRecovering) << 1);
            const uint8 Previous = State->Previous.FindRef(Weak);
            State->Windups += (Current & 1) && !(Previous & 1);
            State->Recoveries += (Current & 2) && !(Previous & 2);
            State->Previous.Add(Weak, Current);
        }
    }), .2f, true, 8.f);
#endif
}

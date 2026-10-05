#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGUI/Manager/PGUIManager.h"

static FAutoConsoleCommandWithWorld PGDashProbe(TEXT("PGDashProbe"),
    TEXT("Verify dash movement, enemy passthrough, wall collision, interruption and pose FX in a Dash_ test profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),Profile) || !Profile.StartsWith(TEXT("Dash_"))) return;
    struct FState
    {
        double Start = FPlatformTime::Seconds();
        double CaptureReady=0.;
        float At=0.f;
        int32 Phase=0, PeakGhosts=0;
        FVector Origin=FVector(20000,20000,148), Forward;
        bool bCaptured=false, bPaused=false, bRequested=false;
        TWeakObjectPtr<UBoxComponent> Wall;
        ECollisionResponse EnemyResponse=ECR_Block;
    };
    auto S=MakeShared<FState>(); TWeakObjectPtr<UWorld> Weak=World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak](float)
    {
        auto* W=Weak.Get(); if(!W) return false;
        auto Finish=[W](bool Pass,const TCHAR* Why)
        {
            UE_LOG(LogTemp,Display,TEXT("PGDashProbe %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),Why);
            UGameplayStatics::SetGamePaused(W,false);
            if(auto* PC=W->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));
            return false;
        };
        if(FPlatformTime::Seconds()-S->Start>90) return Finish(false,TEXT("timeout"));
        auto* P=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        if(!P || !P->GetSkillHandler() || FPlatformTime::Seconds()-S->Start<5) return true;
        auto* Dash=P->GetPlayerDashComponent(); auto* ASC=P->GetPGAbilitySystemComponent();
        auto* Move=P->GetCharacterMovement();
        auto* Capsule=P->GetCapsuleComponent();
        if(S->bPaused)
        {
            if(!S->bRequested)
            {
                if(FPlatformTime::Seconds()<S->CaptureReady) return true;
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("QA/PlayerDash/Dash.png"),false,false);
                S->bRequested=true; return true;
            }
            if(FScreenshotRequest::IsScreenshotRequested()) return true;
            UGameplayStatics::SetGamePaused(W,false); S->bPaused=false;
        }
        if(S->Phase==0)
        {
            if(auto* UI=UPGUIManager::Get(P)) UI->CloseAllUI();
            if(!P->IsGameplayInputAllowed()) return true;
            FString Identity;
            if(FParse::Value(FCommandLine::Get(),TEXT("PGDashCharacter="),Identity))
                if(!UPGProfileSubsystem::Get(P)->SelectCharacter(FName(*Identity))) return Finish(false,TEXT("appearance rejected"));
            auto* Floor=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor);
            Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(2500,2500,50));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent(); Floor->SetActorLocation(FVector(20000,20000,0));
            auto* Surface=NewObject<UStaticMeshComponent>(Floor);
            Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision); Surface->RegisterComponent();
            Surface->SetWorldLocation(Floor->GetActorLocation()); Surface->SetWorldScale3D(FVector(50,50,1));
            P->SetActorLocation(S->Origin,false,nullptr,ETeleportType::TeleportPhysics);
            Move->SetMovementMode(MOVE_Walking); Move->StopMovementImmediately();
            if(auto* Boom=P->FindComponentByClass<USpringArmComponent>()) { Boom->bEnableCameraLag=false; Boom->TargetArmLength=650.f; }
            S->At=W->GetTimeSeconds(); S->Phase=1; return true;
        }
        const float Age=W->GetTimeSeconds()-S->At;
        if(S->Phase==1 && Age>.5f)
        {
            S->EnemyResponse=Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1);
            S->Origin=P->GetActorLocation();
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
            if(!Dash->IsDashing()) return Finish(false,TEXT("GAS activation rejected"));
            S->Forward=P->GetActorForwardVector();
            if(P->CanStartSkill(false) || P->CanStartSkill(true)) return Finish(false,TEXT("dash reentry allowed"));
            S->At=W->GetTimeSeconds(); S->Phase=2; return true;
        }
        if(S->Phase==2)
        {
            S->PeakGhosts=FMath::Max(S->PeakGhosts,Dash->GetVisibleGhostCount());
            if(!S->bCaptured && Age>.20f && FParse::Param(FCommandLine::Get(),TEXT("PGDashCapture")))
            {
                UE_LOG(LogTemp,Display,TEXT("PGDash Capture age=%.3f ghosts=%d"),Age,Dash->GetVisibleGhostCount());
                S->bCaptured=true; S->bPaused=true; S->CaptureReady=FPlatformTime::Seconds(); UGameplayStatics::SetGamePaused(W,true); return true;
            }
            if(Age<.85f) return true;
            const float Distance=FVector::Dist2D(P->GetActorLocation(),S->Origin);
            UE_LOG(LogTemp,Display,TEXT("PGDash Open distance=%.2f ghosts=%d"),Distance,S->PeakGhosts);
            if(Distance<Dash->Distance*.85f || Distance>Dash->Distance*1.12f || S->PeakGhosts<2 || S->PeakGhosts>8 ||
                Dash->IsDashing() || Dash->GetVisibleGhostCount()!=0 || Dash->IsComponentTickEnabled() || !Move->bCanWalkOffLedges ||
                Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1)!=S->EnemyResponse)
                return Finish(false,TEXT("distance, FX lifetime or movement restoration"));
            auto* Obstacle=W->SpawnActor<AActor>(); auto* Wall=NewObject<UBoxComponent>(Obstacle); S->Wall=Wall;
            Obstacle->SetRootComponent(Wall); Wall->SetBoxExtent(FVector(20,300,200));
            Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Wall->SetCollisionResponseToAllChannels(ECR_Block);
            Wall->RegisterComponent(); Obstacle->SetActorLocation(S->Origin+S->Forward*220); Obstacle->SetActorRotation(S->Forward.Rotation());
            P->SetActorLocation(S->Origin,false,nullptr,ETeleportType::TeleportPhysics);
            Move->StopMovementImmediately();
            S->At=W->GetTimeSeconds(); S->Phase=3; return true;
        }
        if(S->Phase==3 && Age>.3f && P->GetSkillHandler()->IsSkillReadyByID(10000))
        {
            // Cursor aim can change after teleport; orient the wall to the actual dash direction.
            const FVector Direction=P->GetDodgeDirection();
            S->Wall->GetOwner()->SetActorLocation(S->Origin+Direction*220);
            S->Wall->GetOwner()->SetActorRotation(Direction.Rotation());
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
            if(!Dash->IsDashing())
            {
                auto* Anim=P->GetMesh()->GetAnimInstance(); auto* Montage=Anim->GetCurrentActiveMontage();
                UE_LOG(LogTemp,Display,TEXT("PGDash Rejected ground=%d allowed=%d canstart=%d cooldown=%.3f montage=%s pos=%.3f"),
                    Move->IsMovingOnGround(),P->IsGameplayInputAllowed(),P->CanStartSkill(true),P->GetSkillHandler()->GetRemainingCooldownByID(10000),*GetNameSafe(Montage),Anim->Montage_GetPosition(Montage));
                return Finish(false,TEXT("second activation rejected"));
            }
            S->At=W->GetTimeSeconds(); S->Phase=4; return true;
        }
        if(S->Phase==4 && Age>.8f && P->GetSkillHandler()->IsSkillReadyByID(10000))
        {
            const float Distance=FVector::Dist2D(P->GetActorLocation(),S->Origin);
            UE_LOG(LogTemp,Display,TEXT("PGDash Wall distance=%.2f"),Distance);
            if(Distance>175.f || Distance<100.f || Dash->IsDashing()) return Finish(false,TEXT("wall penetration"));
            S->Wall->GetOwner()->Destroy();
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
            if(!Dash->IsDashing()) return Finish(false,TEXT("cancel case activation rejected"));
            S->At=W->GetTimeSeconds(); S->Phase=5; return true;
        }
        if(S->Phase==5 && Age>.12f)
        {
            ASC->CancelAbilities(); ASC->ClearBufferedInput();
            S->Origin=P->GetActorLocation();
            if(Dash->IsDashing() || Dash->GetVisibleGhostCount()!=0 || !Move->bCanWalkOffLedges ||
                Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1)!=S->EnemyResponse)
                return Finish(false,TEXT("cancel cleanup"));
            S->At=W->GetTimeSeconds(); S->Phase=6; return true;
        }
        if(S->Phase==6 && Age>.25f)
        {
            if(FVector::Dist2D(P->GetActorLocation(),S->Origin)>5.f) return Finish(false,TEXT("movement leaked after cancel"));
            S->Phase=7; return true;
        }
        if(S->Phase==7 && P->GetSkillHandler()->IsSkillReadyByID(10000))
        {
            S->Origin=P->GetActorLocation(); S->Forward=P->GetDodgeDirection();
            // Use the real EnemyCharacter profile with blocking against the player's
            // actual object channel, so this catches regressions even in overlap-only BPs.
            for(float Offset : {150.f,225.f,300.f})
            {
                auto* Enemy=W->SpawnActor<AActor>(); auto* Body=NewObject<UCapsuleComponent>(Enemy);
                Enemy->SetRootComponent(Body); Body->SetCapsuleSize(34.f,88.f);
                Body->SetCollisionProfileName(TEXT("EnemyCharacter"));
                Body->SetCollisionResponseToChannel(Capsule->GetCollisionObjectType(),ECR_Block);
                Body->RegisterComponent(); Enemy->SetActorLocation(S->Origin+S->Forward*Offset);
            }
            FHitResult Before;
            P->SetActorLocation(S->Origin+S->Forward*Dash->Distance,true,&Before);
            if(!Before.bBlockingHit) return Finish(false,TEXT("enemy fixture did not block normal movement"));
            P->SetActorLocation(S->Origin,false,nullptr,ETeleportType::TeleportPhysics);
            Move->StopMovementImmediately();
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
            if(!Dash->IsDashing()) return Finish(false,TEXT("enemy case activation rejected"));
            S->At=W->GetTimeSeconds(); S->Phase=8; return true;
        }
        if(S->Phase==8 && Age>.85f)
        {
            const float Distance=FVector::DotProduct(P->GetActorLocation()-S->Origin,S->Forward);
            UE_LOG(LogTemp,Display,TEXT("PGDash Enemies distance=%.2f restored=%d"),Distance,
                Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1)==S->EnemyResponse);
            if(Distance<Dash->Distance*.85f || Distance>Dash->Distance*1.12f || Dash->IsDashing() ||
                Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1)!=S->EnemyResponse)
                return Finish(false,TEXT("enemy passthrough or collision restoration"));
            FHitResult After;
            P->SetActorLocation(S->Origin,true,&After);
            if(!After.bBlockingHit) return Finish(false,TEXT("enemy did not block after dash"));
            return Finish(true,TEXT("open wall cancel enemies collision-restoration afterimages cleanup"));
        }
        return true;
    }));
}));
#endif

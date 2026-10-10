#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "EnhancedPlayerInput.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Input/DataAsset_InputConfig.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGUI/Manager/PGUIManager.h"

static FAutoConsoleCommandWithWorld PGPlayerTurnProbe(TEXT("PGPlayerTurnProbe"),
    TEXT("Exercise authored turns through Enhanced Input in an isolated PlayerTurns_ profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),Profile) || !Profile.StartsWith(TEXT("PlayerTurns_"))) return;
    struct FState
    {
        double Started = FPlatformTime::Seconds();
        int32 Phase = 0, Case = 0, Captures = 0;
        float At = 0.f, PeakWeight = 0.f, FootTravel = 0.f;
        float FirstMoveAt = -1.f;
        bool bSawTurn = false, bInterrupted = false;
        FVector Origin = FVector(20000,20000,148), PreviousFoot = FVector::ZeroVector;
        FString Clip;
        TWeakObjectPtr<ACameraActor> Camera;
    };
    auto S = MakeShared<FState>();
    TWeakObjectPtr<UWorld> WeakWorld = World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,WeakWorld](float)
    {
        auto* W = WeakWorld.Get();
        if (!W) return false;
        const auto Finish = [](bool OK, const TCHAR* Reason)
        {
            UE_LOG(LogTemp,Display,TEXT("PGPlayerTurns PREVIEW %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Reason);
            FPlatformMisc::RequestExit(false); return false;
        };
        if (FPlatformTime::Seconds()-S->Started>180.) return Finish(false,TEXT("timeout"));
        auto* P=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        if (!P || !P->GetSkillHandler() || FPlatformTime::Seconds()-S->Started<5.) return true;
        auto* PC=Cast<APlayerController>(P->GetController());
        auto* Input=PC?Cast<UEnhancedPlayerInput>(PC->PlayerInput):nullptr;
        const auto* Config=P->GetInputConfig();
        auto* Action=Config?Config->FindNativeInputActionsByTag(PGGamePlayTags::InputTag_Move):nullptr;
        auto* Anim=P->GetMesh()->GetAnimInstance();
        auto* Move=P->GetCharacterMovement();
        auto* ASC=P->GetPGAbilitySystemComponent();
        auto* Boom=P->FindComponentByClass<USpringArmComponent>();
        if (!Input || !Action || !Anim || !Boom || !P->GetLocomotionData()) return Finish(false,TEXT("missing runtime assets/input"));
        auto* WeightProperty=FindFProperty<FFloatProperty>(Anim->GetClass(),TEXT("LocomotionTurnWeight"));
        if (!WeightProperty) return Finish(false,TEXT("missing graph turn input"));
        const float Now=W->GetTimeSeconds();
        const float Angles[]={-45,-90,-180,45,90,180,180,90,90,90,90,0,45,90,135,180,-135,-90,-45,0,45,90,135,180,-135,-90,-45,90};
        const bool Strafe = S->Case >= 11 && S->Case < 27;
        const float ExpectedSpeed = S->Case >= 11 && S->Case < 19 ? 170.f : 600.f;
        if (S->Phase==0)
        {
            if (auto* UI=UPGUIManager::Get(P)) UI->CloseAllUI();
            if (!P->IsGameplayInputAllowed()) return true;
            FString Identity=TEXT("Bokusei");FParse::Value(FCommandLine::Get(),TEXT("PGTurnCharacter="),Identity);
            if (!UPGProfileSubsystem::Get(P)->SelectCharacter(FName(*Identity))) return Finish(false,TEXT("appearance rejected"));
            auto* Floor=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);
            Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(2500,2500,50));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent();Floor->SetActorLocation(FVector(20000,20000,0));
            auto* Surface=NewObject<UStaticMeshComponent>(Floor);
            Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);Surface->RegisterComponent();
            Surface->SetWorldLocation(Floor->GetActorLocation());Surface->SetWorldScale3D(FVector(50,50,1));
            Boom->bEnableCameraLag=false;Boom->TargetArmLength=400.f;
            Boom->SetWorldRotation(FRotator(-35.f,-45.f,0.f));
            // Freeze only the camera tuning tick; Enhanced Input, timers and movement remain live.
            P->SetActorTickEnabled(false);
            S->Camera=W->SpawnActor<ACameraActor>();
            S->Camera->GetCameraComponent()->SetFieldOfView(40.f);
            PC->SetViewTarget(S->Camera.Get());
            S->Phase=1;S->At=Now;
        }
        if (auto* Camera=S->Camera.Get())
        {
            const FVector Focus=P->GetActorLocation();
            const FVector Position=Focus+FVector(300,-400,220);
            Camera->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
        }
        if (S->Phase==1)
        {
            Input->InjectInputForAction(Action,FInputActionValue(FVector2D::ZeroVector));
            ASC->CancelAllAbilities();ASC->ClearBufferedInput();
            Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Walking);Move->MaxWalkSpeed=600.f;
            P->SetActorLocationAndRotation(S->Origin,FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            if (Now-S->At<1.8f)
            {
                if (S->Case==0 && Now-S->At>1.f && S->Captures==0 && FParse::Param(FCommandLine::Get(),TEXT("PGTurnCapture")))
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("QA/PlayerTurns/Idle.png"),false,false);
                    ++S->Captures;
                }
                return true;
            }
            S->At=Now;S->Phase=2;S->Captures=0;S->PeakWeight=S->FootTravel=0;
            S->bSawTurn=S->bInterrupted=false;S->Clip.Empty();
            S->FirstMoveAt=-1.f;
            S->PreviousFoot=P->GetMesh()->GetSocketLocation(TEXT("foot_l"))-P->GetActorLocation();
            if (S->Case==6) Move->Velocity=FVector(600,0,0);
            if (Strafe)
            {
                P->FaceAimDirection();
                P->SetActorRotation(FRotator::ZeroRotator);
                Move->MaxWalkSpeed=ExpectedSpeed;
            }
        }
        const float Elapsed=Now-S->At;
        const bool Release=S->Case==7 && Elapsed>.12f;
        const float Target=S->Case==8 && Elapsed>.12f ? -90.f : S->Case==27 && Elapsed>.12f ? 45.f : Angles[S->Case];
        if (S->FirstMoveAt<0.f && Move->Velocity.Size2D()>5.f) S->FirstMoveAt=Elapsed;
        const float Radians=FMath::DegreesToRadians(Target-Boom->GetComponentRotation().Yaw);
        Input->InjectInputForAction(Action,FInputActionValue(Release?FVector2D::ZeroVector:FVector2D(FMath::Sin(Radians),FMath::Cos(Radians))));
        if (P->IsLocomotionTurning())
        {
            S->bSawTurn=true;
            S->Clip=P->GetLocomotionTurnAnimation()->GetName();
        }
        S->PeakWeight=FMath::Max(S->PeakWeight,WeightProperty->GetPropertyValue_InContainer(Anim));
        const FVector Foot=P->GetMesh()->GetSocketLocation(TEXT("foot_l"))-P->GetActorLocation();
        S->FootTravel+=FVector::Dist(Foot,S->PreviousFoot);S->PreviousFoot=Foot;
        if (S->Case>=9 && S->Case<=10 && Elapsed>.12f && !S->bInterrupted)
        {
            if (!P->IsLocomotionTurning()) return Finish(false,TEXT("interrupt did not begin during turn"));
            ASC->OnAbilityInputPressed(S->Case==9?PGGamePlayTags::InputTag_Skill_Normal:PGGamePlayTags::InputTag_Roll);
            if (P->IsLocomotionTurning() || (S->Case==9?!P->GetPlayerAttackComponent()->IsRunning():!P->GetPlayerDashComponent()->IsDashing()))
                return Finish(false,TEXT("turn blocked attack/dash"));
            S->bInterrupted=true;
        }
        if (S->Captures<8 && Elapsed>=.025f+S->Captures*.05f && FParse::Param(FCommandLine::Get(),TEXT("PGTurnCapture")))
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("QA/PlayerTurns")/
                FString::Printf(TEXT("Case%02d_%02d.png"),S->Case,S->Captures++),false,false);
        }
        if (Elapsed>1.f)
        {
            UE_LOG(LogTemp,Display,TEXT("PGTurn Case=%d Angle=%.1f Yaw=%.2f Speed=%.2f Weight=%.3f Foot=%.2f Saw=%d Clip=%s"),
                S->Case,Target,P->GetActorRotation().Yaw,Move->Velocity.Size2D(),S->PeakWeight,S->FootTravel,S->bSawTurn,*S->Clip);
            if (Strafe)
            {
                auto* AngleProperty=FindFProperty<FFloatProperty>(Anim->GetClass(),TEXT("LocalVelocityDirectionAngle"));
                const float AnimationAngle=AngleProperty?AngleProperty->GetPropertyValue_InContainer(Anim):999.f;
                if (S->bSawTurn || S->PeakWeight>.01f || S->FootTravel<5.f ||
                    FMath::Abs(P->GetActorRotation().Yaw)>1.f ||
                    FMath::Abs(Move->Velocity.Size2D()-ExpectedSpeed)>5.f ||
                    FMath::Abs(FMath::FindDeltaAngleDegrees(AnimationAngle,Target))>2.f)
                    return Finish(false,TEXT("eight-way combat footwork lost facing/animation direction/speed"));
            }
            else if (S->Case==0 || S->Case==3 || S->Case==6)
            {
                if (S->bSawTurn || S->PeakWeight>.01f) return Finish(false,TEXT("small/running direction change played a planted turn"));
            }
            else if (!S->bSawTurn || S->PeakWeight<.65f || S->FootTravel<5.f) return Finish(false,TEXT("turn pose not evaluated"));
            if (S->Case<=6 && (S->FirstMoveAt<0.f || S->FirstMoveAt>.18f)) return Finish(false,TEXT("turn delayed movement response"));
            if (S->Case<=6 && (FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,Target))>3.f || Move->Velocity.Size2D()<450.f))
                return Finish(false,TEXT("turn did not resume directional locomotion"));
            if (S->Case==7 && (P->IsLocomotionTurning() || Move->Velocity.Size2D()>5.f)) return Finish(false,TEXT("release did not stop turn"));
            if ((S->Case==8 || S->Case==27) && FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,Target))>3.f) return Finish(false,TEXT("redirect lost new input"));
            UE_LOG(LogTemp,Display,TEXT("PGTurn Response Case=%d FirstMoveSeconds=%.3f"),S->Case,S->FirstMoveAt);
            if (++S->Case==UE_ARRAY_COUNT(Angles)) return Finish(true,TEXT("cases=28 enhanced_input=1 attack_interrupt=1 dash_interrupt=1 combat_eight_way=16"));
            S->Phase=1;S->At=Now;
        }
        return true;
    }));
}));
#endif

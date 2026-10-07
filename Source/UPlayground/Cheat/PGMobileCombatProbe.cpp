#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "EnhancedPlayerInput.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Input/DataAsset_InputConfig.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGUI/Manager/PGUIManager.h"

// Exercises the saved loadout through Enhanced Input, CharacterMovement and real GAS.
// The generated profile and isolated platform are confined to this disposable process.
static FAutoConsoleCommandWithWorld PGMobileCombatProbe(
    TEXT("PGMobileCombatProbe"), TEXT("Validate walking attacks and buffered dash exits in a MobileCombat_ test profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile) ||
        !Profile.StartsWith(TEXT("MobileCombat_"))) return;
    struct FState
    {
        double Start = FPlatformTime::Seconds();
        float At = 0.f, CastAt = 0.f, MinimumSpeed = 10000.f, LegAngle = 0.f;
        int32 Phase = 0, Index = 0, FrozenFrames = 0;
        bool bFreezeRequested = false, bCaptured = false;
        FVector Origin = FVector(20000,20000,148), CastOrigin = FVector::ZeroVector, Direction = FVector::ZeroVector;
        FQuat PreviousLeg = FQuat::Identity;
        TWeakObjectPtr<AActor> Wall;
    };
    auto S = MakeShared<FState>();
    TWeakObjectPtr<UWorld> WeakWorld = World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S, WeakWorld](float)
    {
        auto* W = WeakWorld.Get();
        if (!W) return false;
        auto Finish = [W](bool OK, const TCHAR* Reason)
        {
            UE_LOG(LogTemp, Display, TEXT("PGMobileCombatProbe %s %s"), OK ? TEXT("PASS") : TEXT("FAIL"), Reason);
            FPlatformMisc::RequestExit(false);
            return false;
        };
        if (FPlatformTime::Seconds() - S->Start > 90.) return Finish(false, TEXT("timeout"));
        auto* P = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        if (!P || !P->GetSkillHandler() || FPlatformTime::Seconds() - S->Start < 4.) return true;
        auto* PC = Cast<APlayerController>(P->GetController());
        auto* Input = PC ? Cast<UEnhancedPlayerInput>(PC->PlayerInput) : nullptr;
        const auto* Config = P->GetInputConfig();
        auto* Action = Config ? Config->FindNativeInputActionsByTag(PGGamePlayTags::InputTag_Move) : nullptr;
        if (!Input || !Action) return Finish(false, TEXT("missing Enhanced Input move action"));
        auto* Move = P->GetCharacterMovement();
        auto* ASC = P->GetPGAbilitySystemComponent();
        auto* Attack = P->GetPlayerAttackComponent();
        auto* Dash = P->GetPlayerDashComponent();
        auto* Anim = P->GetMesh()->GetAnimInstance();
        auto* Handler = P->GetSkillHandler();
        const float Now = W->GetTimeSeconds();
        const int32 Skills[] = {100,101,102,112,114};
        const float Ratios[] = {1.f,1.f,1.f,.85f,.9f};
        const auto Equip = [&](int32 ID)
        {
            ASC->ClearBufferedInput();
            Handler->RemoveSkill(EPGSkillSlot::SkillSlot_1);
            Handler->AddSkill(EPGSkillSlot::SkillSlot_1,ID);
            Handler->GetSkillData(EPGSkillSlot::SkillSlot_1)->CoolTime = 0.f;
        };
        if (S->Phase == 0)
        {
            if (auto* UI = UPGUIManager::Get(P)) UI->CloseAllUI();
            if (!P->IsGameplayInputAllowed() || !Anim) return true;
            FString Identity;
            if (FParse::Value(FCommandLine::Get(),TEXT("PGMobileCharacter="),Identity))
                if (!UPGProfileSubsystem::Get(P)->SelectCharacter(FName(*Identity))) return Finish(false,TEXT("appearance rejected"));
            auto* Floor = W->SpawnActor<AActor>();
            auto* Box = NewObject<UBoxComponent>(Floor);
            Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(2500,2500,50));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent(); Floor->SetActorLocation(FVector(20000,20000,0));
            auto* Surface = NewObject<UStaticMeshComponent>(Floor);
            Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision); Surface->RegisterComponent();
            Surface->SetWorldLocation(Floor->GetActorLocation()); Surface->SetWorldScale3D(FVector(50,50,1));
            auto* Boom = P->FindComponentByClass<USpringArmComponent>();
            if (!Boom) return Finish(false,TEXT("missing camera"));
            Boom->bEnableCameraLag=false; Boom->TargetArmLength=700.f;
            S->Direction = FRotator(0,Boom->GetComponentRotation().Yaw,0).RotateVector(FVector::RightVector);
            S->Phase=1; S->At=Now;
            P->SetActorLocation(S->Origin,false,nullptr,ETeleportType::TeleportPhysics);
            Move->SetMovementMode(MOVE_Walking); Move->MaxWalkSpeed=600.f;
            Equip(Skills[0]);
            return true;
        }
        // Inject the same action bound to WASD, including the character's input-direction state.
        Input->InjectInputForAction(Action,FInputActionValue(FVector2D(1,0)));
        if (S->Phase == 1 && Now-S->At > 1.2f)
        {
            const float Before = Move->Velocity.Size2D();
            if (Before < 450.f)
            {
                UE_LOG(LogTemp,Display,TEXT("PGMobile Input Allowed=%d Value=%s Last=%s Pending=%s Speed=%.2f Max=%.2f Mode=%d"),
                    P->IsGameplayInputAllowed(),*Input->GetActionValue(Action).ToString(),*P->GetLastMovementInputVector().ToString(),
                    *P->GetPendingMovementInputVector().ToString(),Before,Move->MaxWalkSpeed,int32(Move->MovementMode));
                return Finish(false,TEXT("WASD injection did not establish walking"));
            }
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
            if (!Attack->IsRunning()) return Finish(false,TEXT("walking skill did not activate"));
            if (Move->Velocity.Size2D() < Before*.95f) return Finish(false,TEXT("activation erased walking velocity"));
            if (!FMath::IsNearlyEqual(Move->MaxWalkSpeed,600.f*Ratios[S->Index],.1f))
                return Finish(false,TEXT("activation frame has incorrect walking speed"));
            auto* Montage = Anim->GetCurrentActiveMontage();
            if (!Montage || Montage->SlotAnimTracks.Num()!=1 ||
                Montage->SlotAnimTracks[0].SlotName != FName(Skills[S->Index]==112 ? TEXT("FullBody") : TEXT("UpperBody")))
                return Finish(false,TEXT("incorrect movement/attack animation slot"));
            S->CastOrigin=P->GetActorLocation(); S->CastAt=Now; S->MinimumSpeed=Before;
            S->LegAngle=0.f; S->bFreezeRequested=false; S->FrozenFrames=0; S->bCaptured=false;
            const int32 LegIndex=P->GetMesh()->GetBoneIndex(TEXT("Thigh_L"));
            if (!P->GetMesh()->GetBoneSpaceTransforms().IsValidIndex(LegIndex)) return Finish(false,TEXT("missing lower-body pose"));
            S->PreviousLeg=P->GetMesh()->GetBoneSpaceTransforms()[LegIndex].GetRotation();
            S->Phase=2;
        }
        else if (S->Phase == 2)
        {
            S->MinimumSpeed=FMath::Min(S->MinimumSpeed,float(Move->Velocity.Size2D()));
            const FQuat Leg=P->GetMesh()->GetBoneSpaceTransforms()[P->GetMesh()->GetBoneIndex(TEXT("Thigh_L"))].GetRotation();
            S->LegAngle+=FMath::RadiansToDegrees(S->PreviousLeg.AngularDistance(Leg)); S->PreviousLeg=Leg;
            if (!S->bFreezeRequested && Attack->IsRunning() && Attack->GetLogicalTime() > .2f)
            {
                // Hold two evaluated frames to test the production hit-stop branch.
                Anim->GetSkelMeshComponent()->GlobalAnimRateScale=0.f; S->bFreezeRequested=true;
            }
            else if (S->bFreezeRequested && P->GetMesh()->GlobalAnimRateScale<=0.f && ++S->FrozenFrames>=2)
                P->GetMesh()->GlobalAnimRateScale=1.f;
            if (!S->bCaptured && Now-S->CastAt>.35f && FParse::Param(FCommandLine::Get(),TEXT("PGMobileCapture")))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("QA/MobileCombat")/
                    FString::Printf(TEXT("Skill%d.png"),Skills[S->Index]),false,false);
                S->bCaptured=true;
            }
            if (!Attack->IsRunning())
            {
                const float Distance=FVector::DotProduct(P->GetActorLocation()-S->CastOrigin,S->Direction);
                UE_LOG(LogTemp,Display,TEXT("PGMobile Skill=%d Distance=%.2f MinSpeed=%.2f LegAngle=%.2f FreezeFrames=%d ExitSpeed=%.2f"),
                    Skills[S->Index],Distance,S->MinimumSpeed,S->LegAngle,S->FrozenFrames,Move->Velocity.Size2D());
                if (Distance<250.f || S->MinimumSpeed<350.f || Move->Velocity.Size2D()<350.f || S->LegAngle<3.f || S->FrozenFrames<2)
                    return Finish(false,TEXT("walking, lower-body animation or hit-stop continuity failed"));
                if (++S->Index<UE_ARRAY_COUNT(Skills))
                {
                    P->SetActorLocation(S->Origin,false,nullptr,ETeleportType::TeleportPhysics);
                    Equip(Skills[S->Index]); S->At=Now; S->Phase=1;
                }
                else
                {
                    P->SetActorLocation(S->Origin,false,nullptr,ETeleportType::TeleportPhysics);
                    Equip(114); S->At=Now; S->Phase=3;
                }
            }
        }
        else if (S->Phase == 3 && Now-S->At>1.2f)
        {
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
            if (!Dash->IsDashing()) return Finish(false,TEXT("dash did not start"));
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
            if (Attack->IsRunning()) return Finish(false,TEXT("attack interrupted dash displacement"));
            S->At=Now; S->Phase=4;
        }
        else if (S->Phase == 4 && !Dash->IsDashing())
        {
            if (!Attack->IsRunning())
            {
                if (Now-S->At>.7f) return Finish(false,TEXT("early dash input expired before exit"));
                return true;
            }
            UE_LOG(LogTemp,Display,TEXT("PGMobile DashExit Delay=%.3f Skill=%d WalkSpeed=%.2f"),
                Now-S->At,Attack->GetCastContext()->SkillID,Move->MaxWalkSpeed);
            if (Attack->GetCastContext()->SkillID!=114 || Move->MaxWalkSpeed<500.f || Move->Velocity.Size2D()<350.f)
                return Finish(false,TEXT("dash exit selected wrong skill or retained movement lock"));
            ASC->CancelAllAbilities(); ASC->ClearBufferedInput();
            P->SetActorLocation(S->Origin,false,nullptr,ETeleportType::TeleportPhysics);
            auto* Wall=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
            Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(80,80,200));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent(); Wall->SetActorLocation(S->Origin+S->Direction*280.f);
            S->Wall=Wall; Equip(112); ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
            S->At=Now; S->Phase=5;
        }
        else if (S->Phase==5 && Now-S->At>1.f)
        {
            const float Travel=FVector::DotProduct(P->GetActorLocation()-S->Origin,S->Direction);
            if (Travel>230.f || Travel<20.f || !Attack->IsRunning()) return Finish(false,TEXT("walking attack bypassed wall collision"));
            UE_LOG(LogTemp,Display,TEXT("PGMobile Wall Travel=%.2f CastRunning=1"),Travel);
            ASC->CancelAllAbilities(); ASC->ClearBufferedInput();
            if (Attack->IsRunning() || !FMath::IsNearlyEqual(Move->MaxWalkSpeed,600.f,.1f))
                return Finish(false,TEXT("cancel did not restore movement state"));
            return Finish(true,TEXT("skills=5 enhanced_move=1 dash_buffer=1 wall_collision=1 lower_body=1"));
        }
        return true;
    }));
}));
#endif

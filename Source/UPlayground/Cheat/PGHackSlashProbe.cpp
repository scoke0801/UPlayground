#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGUI/Manager/PGUIManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "UObject/UObjectIterator.h"

static FAutoConsoleCommandWithWorld PGHackSlashLoadout(TEXT("PGHackSlashLoadout"),
    TEXT("Equip 111 + 112 without saving in a HackSlash_ test profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),Profile) || !Profile.StartsWith(TEXT("HackSlash_"))) return;
    auto* Player=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World,0));
    if (!Player || !Player->GetSkillHandler() || Player->GetPlayerAttackComponent()->IsRunning()) return;
    auto* Handler=Player->GetSkillHandler();
    for (auto Slot : {EPGSkillSlot::SkillSlot_1,EPGSkillSlot::SkillSlot_2}) Handler->RemoveSkill(Slot);
    Handler->AddSkill(EPGSkillSlot::SkillSlot_1,111); Handler->AddSkill(EPGSkillSlot::SkillSlot_2,112);
    Player->GetPlayerAttackComponent()->PrepareLoadout();
    Handler->ResetCombo();
    if(auto* Messages=UPGMessageManager::Get(Player)) Messages->SendMessage(EPGPlayerMessageType::LoadoutChanged,nullptr);
    UE_LOG(LogTemp,Display,TEXT("PGHackSlashLoadout ready 111+112 (test profile, not saved)"));
}));

// Real GAS activation and world collision queries; no weapon contact injection.
static FAutoConsoleCommandWithWorld PGHackSlashProbe(TEXT("PGHackSlashProbe"),
    TEXT("Run P0 spatial attacks in a disposable HackSlash_ test profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile;
    if (!World || !FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile) || !Profile.StartsWith(TEXT("HackSlash_"))) return;
    struct FProbe
    {
        double Started = FPlatformTime::Seconds(), CastAt=0;
        float CaptureAt=0.f;
        int32 Index=0;
        bool bReady=false, bPlaced=false, bCaptured=false;
        bool bCapturePaused=false;
        bool bCaptureRequested=false;
        double CaptureReadyAt=0.;
        bool bP1=FParse::Param(FCommandLine::Get(),TEXT("PGHackSlashP1Probe"));
        TArray<int32> Skills={100,101,102,111,112};
        TArray<TWeakObjectPtr<APGCharacterEnemy>> Targets;
        TSharedPtr<FPGSkillCastContext> Context;
        FVector Origin;
        TArray<FTransform> PreviousPose;
        bool bMotionTrace=FParse::Param(FCommandLine::Get(),TEXT("PGHackSlashMotionTrace"));
        bool bCombo=FParse::Param(FCommandLine::Get(),TEXT("PGHackSlashComboProbe"));
        bool bComboStarted=false;
        int32 ComboCount=0, OutgoingID=INDEX_NONE, CrossfadeSamples=0;
        float OutgoingPosition=0.f;
        double LastComboAt=0.;
        TArray<int32> ComboSkills;
    };
    auto State=MakeShared<FProbe>(); TWeakObjectPtr<UWorld> WeakWorld=World;
    if (State->bP1) State->Skills={110,113,114};
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,WeakWorld](float)
    {
        auto* W=WeakWorld.Get(); if(!W) return false;
        // Screenshot readback can stall the first captured frame long enough to
        // expire a short FX. Hold only capture runs until the requested frame is consumed.
        if (State->bCapturePaused)
        {
            if (!State->bCaptureRequested)
            {
                if (FPlatformTime::Seconds()<State->CaptureReadyAt) return true;
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("QA/HackSlashP0")/
                    FString::Printf(TEXT("Skill_%d.png"),State->Skills[State->Index]),false,false);
                State->bCaptureRequested=true;
                return true;
            }
            if (FScreenshotRequest::IsScreenshotRequested()) return true;
            UGameplayStatics::SetGamePaused(W,false);
            State->bCapturePaused=false;
            return true;
        }
        const auto Finish=[](bool bOK,const TCHAR* Reason)
        { UE_LOG(LogTemp,Display,TEXT("PGHackSlashProbe %s %s"),bOK?TEXT("PASS"):TEXT("FAIL"),Reason); FPlatformMisc::RequestExit(false); return false; };
        if(FPlatformTime::Seconds()-State->Started>45) return Finish(false,TEXT("timeout"));
        auto* Player=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        if(!Player || !Player->GetSkillHandler() || !Player->GetMesh()->GetAnimInstance()) return true;
        auto* ASC=Player->GetPGAbilitySystemComponent(); auto* Attack=Player->GetPlayerAttackComponent();
        auto* Handler=Player->GetSkillHandler();
        if (State->bMotionTrace && !State->bCombo && State->Context && Attack->IsRunning())
        {
            const auto& Pose=Player->GetMesh()->GetBoneSpaceTransforms();
            float MaxAngle=0.f;
            int32 MaxBone=0;
            if (State->PreviousPose.Num()==Pose.Num())
                for (int32 Bone=0;Bone<Pose.Num();++Bone)
                {
                    const float Angle=FMath::RadiansToDegrees(Pose[Bone].GetRotation().AngularDistance(State->PreviousPose[Bone].GetRotation()));
                    if (Angle>MaxAngle) { MaxAngle=Angle; MaxBone=Bone; }
                }
            auto* Anim=Player->GetMesh()->GetAnimInstance();
            auto* Montage=Anim->GetCurrentActiveMontage();
            UE_LOG(LogTemp,Display,TEXT("PGMotion Skill=%d Frame=%llu DT=%.6f Clock=%.6f Position=%.6f Frozen=%d MaxBoneAngle=%.6f Bones=%d MaxBone=%s"),
                State->Context->SkillID,GFrameCounter,W->GetDeltaSeconds(),Attack->GetLogicalTime(),Anim->Montage_GetPosition(Montage),
                Player->GetMesh()->GlobalAnimRateScale<=0.f,MaxAngle,Pose.Num(),*Player->GetMesh()->GetBoneName(MaxBone).ToString());
            State->PreviousPose=Pose;
        }
        if(!State->bReady)
        {
            if(FPlatformTime::Seconds()-State->Started<4) return true;
            FPGStagePresentation View; View.bClose=true;
            if(auto* Messages=UPGMessageManager::Get(Player)) Messages->SendMessage(EPGUIMessageType::StagePresentation,&View);
            if(auto* UI=UPGUIManager::Get(Player)) UI->CloseAllUI();
            if(!Player->IsGameplayInputAllowed()) return true;
            if (State->bP1)
            {
                auto* Saves=UPGProfileSubsystem::Get(Player);
                if (!Saves || !Saves->SelectActiveSkills(110,114)) return Finish(false,TEXT("safe preparation selection rejected"));
                Handler->UseSkill(EPGSkillSlot::SkillSlot_1);
                const float Remaining=Handler->GetRemainingCooldownByID(110);
                if (!Saves->SelectActiveSkills(114,110) || !FMath::IsNearlyEqual(Handler->GetRemainingCooldownByID(110),Remaining,.05f))
                    return Finish(false,TEXT("slot swap reset cooldown"));
                const auto* Snapshot=Saves->GetProfile();
                Saves->bInjectSaveFailure=true;
                const bool Accepted=Saves->SelectActiveSkills(111,113);
                Saves->bInjectSaveFailure=false;
                if (Accepted || Saves->GetProfile()!=Snapshot || Handler->GetSkillData(EPGSkillSlot::SkillSlot_1)->SkillId!=114 || Handler->GetSkillData(EPGSkillSlot::SkillSlot_2)->SkillId!=110)
                    return Finish(false,TEXT("failed save changed committed loadout"));
                if (Saves->SelectActiveSkills(110,110)) return Finish(false,TEXT("duplicate loadout accepted"));
                Handler->RemoveSkill(EPGSkillSlot::SkillSlot_2);
                UE_LOG(LogTemp,Display,TEXT("PGHackSlashP1 Loadout PASS safe_stage=1 swap_cooldown=1 failed_save_atomic=1 duplicate_rejected=1"));
            }
            auto* Floor=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor);
            Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(2500,2500,50));
            Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Floor->SetActorLocation(FVector(20000,20000,0));
            if(FParse::Param(FCommandLine::Get(),TEXT("PGHackSlashCapture")))
            {
                auto* Surface=NewObject<UStaticMeshComponent>(Floor);
                Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
                Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision); Surface->RegisterComponent();
                Surface->SetWorldLocation(Floor->GetActorLocation()); Surface->SetWorldScale3D(FVector(50,50,1));
            }
            State->Origin=FVector(20000,20000,50+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
            if(auto* Boom=Player->FindComponentByClass<USpringArmComponent>()) Boom->bEnableCameraLag=false;
            ASC->SetEquipmentBonuses({}); ASC->SetProfileBonuses({}); ASC->SetCombatPerks({});
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetAttackPowerAttribute(),100);
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute(),0);
            State->bReady=true;
        }
        if (State->bCombo)
        {
            auto* Anim=Player->GetMesh()->GetAnimInstance();
            if (!State->bComboStarted)
            {
                State->bComboStarted=true;
                Player->SetActorLocation(State->Origin,false,nullptr,ETeleportType::TeleportPhysics);
                Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                Handler->ResetCombo();
                ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Normal);
            }
            const auto Current=Attack->GetCastContext();
            if (Current && Current!=State->Context)
            {
                State->Context=Current;
                State->ComboSkills.Add(Current->SkillID); ++State->ComboCount;
                State->LastComboAt=W->GetTimeSeconds();
                UE_LOG(LogTemp,Display,TEXT("PGCombo Begin=%d Skill=%d World=%.6f"),State->ComboCount,Current->SkillID,W->GetTimeSeconds());
                auto* Incoming=Anim->GetActiveInstanceForMontage(Anim->GetCurrentActiveMontage());
                if (!Incoming || Incoming->GetPlayRate()!=0.f)
                    return Finish(false,TEXT("combo incoming montage is not profile sampled"));
                if (State->ComboCount==4)
                {
                    if (State->ComboSkills!=TArray<int32>{100,101,102,100}) return Finish(false,TEXT("combo order mismatch"));
                    ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_Skill_Normal);
                }
            }
            if (auto* Outgoing=Anim->GetMontageInstanceForID(State->OutgoingID))
            {
                if (Outgoing->GetPlayRate()!=0.f || !FMath::IsNearlyEqual(Outgoing->GetPosition(),State->OutgoingPosition,.001f))
                    return Finish(false,TEXT("outgoing sampled pose resumed during crossfade"));
                auto* Incoming=Anim->GetActiveInstanceForMontage(Anim->GetCurrentActiveMontage());
                if (Incoming && Incoming->GetWeight()>0.f && Outgoing->GetWeight()>0.f)
                {
                    ++State->CrossfadeSamples;
                    if (Incoming->GetWeight()+Outgoing->GetWeight()<.95f)
                        return Finish(false,TEXT("combo crossfade dropped through locomotion"));
                    UE_LOG(LogTemp,Display,TEXT("PGCombo Blend In=%.6f Out=%.6f Position=%.6f"),
                        Incoming->GetWeight(),Outgoing->GetWeight(),Outgoing->GetPosition());
                }
            }
            if (State->bMotionTrace)
            {
                const auto& Pose=Player->GetMesh()->GetBoneSpaceTransforms();
                float MaxAngle=0.f;
                if (State->PreviousPose.Num()==Pose.Num())
                    for (int32 Bone=0;Bone<Pose.Num();++Bone)
                        MaxAngle=FMath::Max(MaxAngle,float(FMath::RadiansToDegrees(Pose[Bone].GetRotation().AngularDistance(State->PreviousPose[Bone].GetRotation()))));
                UE_LOG(LogTemp,Display,TEXT("PGCombo Pose Frame=%llu Skill=%d Clock=%.6f MaxBoneAngle=%.6f Bones=%d"),
                    GFrameCounter,Current?Current->SkillID:0,Attack->GetLogicalTime(),MaxAngle,Pose.Num());
                State->PreviousPose=Pose;
            }
            if (State->ComboCount==4 && !Attack->IsRunning() && W->GetTimeSeconds()-State->LastComboAt>1.5)
            {
                if (Anim->GetCurrentActiveMontage() || State->CrossfadeSamples<6)
                    return Finish(false,TEXT("combo release cleanup or crossfade samples missing"));
                return Finish(true,TEXT("combo=100,101,102,100 sampled_crossfade=1 release=1"));
            }
            if (State->ComboCount<4)
            {
                auto* Outgoing=Anim->GetActiveInstanceForMontage(Anim->GetCurrentActiveMontage());
                const int32 OldID=Outgoing?Outgoing->GetInstanceID():INDEX_NONE;
                const float OldPosition=Outgoing?Outgoing->GetPosition():0.f;
                ASC->OnAbilityInputHeld(PGGamePlayTags::InputTag_Skill_Normal);
                if (Attack->GetCastContext()!=Current)
                { State->OutgoingID=OldID; State->OutgoingPosition=OldPosition; }
            }
            return true;
        }
        if(!State->Context)
        {
            if(State->Index>=State->Skills.Num()) return Finish(true,State->bP1 ? TEXT("skills=3 p1=1 real_spatial=1 contact_injected=0 source_attack=100") : TEXT("skills=5 real_spatial=1 contact_injected=0 source_attack=100"));
            Player->SetActorLocation(State->Origin,false,nullptr,ETeleportType::TeleportPhysics);
            Player->GetCharacterMovement()->StopMovementImmediately();
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Handler->RemoveSkill(EPGSkillSlot::SkillSlot_1); Handler->AddSkill(EPGSkillSlot::SkillSlot_1,State->Skills[State->Index]);
            if (State->bP1)
            {
                // The preceding loadout test deliberately starts a cooldown. Reset only this isolated measurement.
                auto* Skill=Handler->GetSkillData(EPGSkillSlot::SkillSlot_1);
                Skill->LastSkillUsedTime=-1.e30; Skill->InheritedCooldownUntil=-1.e30;
            }
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
            State->Context=Attack->GetCastContext();
            if(!State->Context) return Finish(false,TEXT("profile cast did not commit"));
            State->CastAt=FPlatformTime::Seconds(); State->bPlaced=false; State->bCaptured=false;
            if (const auto* Row=UPGDataTableManager::Get(Player)->GetRowData<FPGSkillDataRow>(State->Skills[State->Index]))
                if (const auto* SkillProfile=Row->PlayerProfile.LoadSynchronous(); SkillProfile && !SkillProfile->HitPhases.IsEmpty())
                    State->CaptureAt=SkillProfile->HitPhases[0].Start+.04f;
            State->PreviousPose.Reset();
            return true;
        }
        if(!State->bPlaced && Attack->GetLogicalTime()>=.1f)
        {
            const int32 Skill=State->Skills[State->Index];
            const FVector F=Player->GetActorForwardVector(), R=Player->GetActorRightVector();
            for(int32 Index=0;Index<2;++Index)
            {
                const FVector Offset=Skill==110 ? F*800.f+R*(Index?100.f:-100.f) : Skill==114 ? F*(Index?700.f:30.f) : Skill==111 ? F*(Index?550.f:220.f)+R*110.f : F*(Index?-150.f:150.f);
                auto* Target=W->SpawnActor<APGCharacterEnemy>(APGCharacterEnemy::StaticClass(),State->Origin+Offset,FRotator::ZeroRotator);
                if(!Target) return Finish(false,TEXT("target spawn failed"));
                Target->bCanDropLoot=false; Target->GetCharacterMovement()->DisableMovement();
                Target->GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel1);
                Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                auto* TargetASC=Target->GetPGAbilitySystemComponent(); TargetASC->InitAbilityActorInfo(Target,Target);
                TargetASC->InitializeCombatStats({{EPGStatType::Health,10000},{EPGStatType::Defense,0}});
                TargetASC->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(),10000.f);
                TargetASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),10000.f);
                TargetASC->SetNumericAttributeBase(UPGAtrributeSet::GetDefensePowerAttribute(),0.f);
                State->Targets.Add(Target);
            }
            State->bPlaced=true;
        }
        if(!State->bCaptured && Attack->GetLogicalTime()>=State->CaptureAt && State->Context->HitTargets.Num()>0 && FParse::Param(FCommandLine::Get(),TEXT("PGHackSlashCapture")))
        {
            if (FParse::Param(FCommandLine::Get(),TEXT("PGNiagaraSlashProbe")))
            {
                int32 Systems = 0;
                for (TObjectIterator<UNiagaraComponent> It; It; ++It)
                    if (It->GetWorld() == W && It->IsActive() && It->GetAsset() &&
                        It->GetAsset()->GetPathName().StartsWith(TEXT("/Game/Art/PlayerCombatFX/")))
                    {
                        auto Controller = It->GetSystemInstanceController();
                        if (!Controller || Controller->GetAge() <= 0.f || It->GetAgeUpdateMode() != ENiagaraAgeUpdateMode::DesiredAge)
                            return Finish(false,TEXT("Niagara slash has not simulated"));
                        ++Systems;
                        UE_LOG(LogTemp,Display,TEXT("PGPlayerNiagara Skill=%d System=%s Age=%.4f DesiredAge=%.4f Scale=%s"),
                            State->Skills[State->Index], *It->GetAsset()->GetName(), Controller->GetAge(),
                            It->GetDesiredAge(), *It->GetComponentScale().ToString());
                    }
                if (!Systems) return Finish(false,TEXT("Niagara slash missing at hit"));
            }
            State->bCaptured=true;
            State->bCapturePaused=UGameplayStatics::SetGamePaused(W,true);
            if (!State->bCapturePaused) return Finish(false,TEXT("capture pause rejected"));
            State->bCaptureRequested=false;
            // Render several frames of the held state before reading the backbuffer.
            State->CaptureReadyAt=FPlatformTime::Seconds()+.35;
        }
        if(!Attack->IsRunning() && (!State->bP1 || FPlatformTime::Seconds()-State->CastAt>1.4))
        {
            if (FParse::Param(FCommandLine::Get(),TEXT("PGNiagaraSlashProbe")))
            {
                for (TObjectIterator<UNiagaraComponent> It; It; ++It)
                    if (It->GetWorld() == W && It->IsActive() && It->GetAsset() &&
                        It->GetAsset()->GetPathName().StartsWith(TEXT("/Game/Art/PlayerCombatFX/")))
                        return Finish(false,TEXT("Niagara slash survived cast/projectile cleanup"));
                UE_LOG(LogTemp,Display,TEXT("PGPlayerNiagara Cleanup Skill=%d PASS"), State->Skills[State->Index]);
            }
            if(!State->bPlaced) return Finish(false,TEXT("cast ended before first phase"));
            const int32 Skill=State->Skills[State->Index];
            const float Power=Skill==110?220.f:Skill==113?285.f:Skill==114?160.f:Skill==100?90.f:Skill==102?150.f:Skill==111?90.f:Skill==112?200.f:100.f;
            for(int32 Index=0;Index<State->Targets.Num();++Index)
            {
                auto* Target=State->Targets[Index].Get(); if(!Target) return Finish(false,TEXT("target destroyed"));
                const float Damage=10000-Target->GetPGAbilitySystemComponent()->GetHealth();
                const float Expected=Index==1 && (Skill<110 || Skill==113)?0.f:Power;
                UE_LOG(LogTemp,Display,TEXT("PGHackSlashProbe Skill=%d Target=%d Damage=%.1f Expected=%.1f Move=%.1f Phases=%d"),
                    Skill,Index,Damage,Expected,FVector::Dist2D(Player->GetActorLocation(),State->Origin),State->Context->HitTargets.Num());
                if(!FMath::IsNearlyEqual(Damage,Expected,.1f)) return Finish(false,TEXT("spatial damage mismatch"));
                Target->Destroy();
            }
            State->Targets.Reset(); State->Context.Reset(); ++State->Index;
        }
        return true;
    }));
}));
#endif

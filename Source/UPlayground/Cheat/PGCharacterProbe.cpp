#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Rendering/PGCharacterAppearanceComponent.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/PGDataTableManager.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGUI/Widget/Window/PGUIInventory.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

static FAutoConsoleCommandWithWorld PGCharacterProbe(TEXT("PGCharacterProbe"),
    TEXT("Validate character selection, retargeted combat and P09 spawning in a disposable Characters_ profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString ProfileName;
    if (!World || !FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),ProfileName) || !ProfileName.StartsWith(TEXT("Characters_"))) return;
    struct FState
    {
        double Started = FPlatformTime::Seconds();
        double PhaseAt = 0;
        int32 Index = 0, Phase = 0;
        float Travel = 0;
        bool bCaptured = false;
        TArray<FTransform> InitialPose;
        TArray<int32> SampleSourceBones, SampleTargetBones;
        TArray<FString> PoseSamples;
        TWeakObjectPtr<APGCharacterEnemy> Enemy;
    };
    auto State = MakeShared<FState>();
    State->PoseSamples.Add(TEXT("identity,seconds,logical_seconds,bone,source_x,source_y,source_z,target_x,target_y,target_z,target_sx,target_sy,target_sz"));
    TWeakObjectPtr<UWorld> WeakWorld = World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,WeakWorld](float)
    {
        auto* W = WeakWorld.Get(); if (!W) return false;
        const auto Finish=[State](bool OK,const TCHAR* Reason)
        {
            FString Output;
            if (!FParse::Value(FCommandLine::Get(),TEXT("PGCharacterProbeOutput="),Output))
                Output = FPaths::ProjectSavedDir()/TEXT("PlayableCharacters/pose-samples.csv");
            OK &= FFileHelper::SaveStringArrayToFile(State->PoseSamples, *Output);
            UE_LOG(LogTemp,Display,TEXT("PGCharacterProbe Samples=%d CSV=%s"),State->PoseSamples.Num()-1,*Output);
            UE_LOG(LogTemp,Display,TEXT("PGCharacterProbe %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Reason);
            FPlatformMisc::RequestExit(false); return false;
        };
        const double Now = FPlatformTime::Seconds();
        if (Now-State->Started>150) return Finish(false,TEXT("timeout"));
        auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        auto* Profile = UPGProfileSubsystem::Get(W);
        if (!Player || !Profile || !Profile->GetCatalog() || !Player->GetSkillHandler() || Now-State->Started<4) return true;
        const auto& Catalog = Profile->GetCatalog()->PlayableCharacters;
        if (Catalog.Num()<2) return Finish(false,TEXT("expected multiple playable identities"));
        if (State->Phase==0)
        {
            if (FParse::Param(FCommandLine::Get(),TEXT("PGCharacterCapture")))
                if (auto* Boom=Player->FindComponentByClass<USpringArmComponent>())
                { Boom->TargetArmLength=550.f; Boom->SetRelativeRotation(FRotator(-25.f,-45.f,0)); }
            FPGStagePresentation View; View.bClose=true;
            if(auto* Messages=UPGMessageManager::Get(Player)) Messages->SendMessage(EPGUIMessageType::StagePresentation,&View);
            if(auto* UI=UPGUIManager::Get(Player)) UI->CloseAllUI();
            FString Reason;
            if (!Profile->CanChangeSkills(Reason)) return true;
            auto* Appearance=Catalog[State->Index].LoadSynchronous();
            const auto* Before=Profile->GetProfile();
            auto* BeforeAppearance=Player->AppearanceComponent->GetAppearance();
            if (!Appearance) return Finish(false,TEXT("missing appearance"));
            Profile->bInjectSaveFailure=true;
            const bool Accepted=Profile->SelectCharacter(Appearance->Id);
            Profile->bInjectSaveFailure=false;
            if (Accepted || Profile->GetProfile()!=Before || Player->AppearanceComponent->GetAppearance()!=BeforeAppearance)
                return Finish(false,TEXT("failed save changed character"));
            if (Profile->SelectCharacter(TEXT("NotInCatalog"))) return Finish(false,TEXT("unknown identity accepted"));
            if (!Profile->SelectCharacter(Appearance->Id)) return Finish(false,TEXT("safe selection rejected"));
            auto* Mesh=Player->AppearanceComponent->GetPresentationMesh();
            if (!Mesh || !Mesh->IsVisible() || Player->GetMesh()->IsVisible() || !Mesh->GetAnimInstance())
                return Finish(false,TEXT("missing visible retarget mesh"));
            // Exercise explicit grips without moving the actual gameplay weapon or
            // saving the asset. Reusing a socket must not reuse another tag's offset.
            const FName* Hand = Appearance->EquipmentBones.Find(TEXT("hand_r"));
            if (!Hand) return Finish(false,TEXT("missing right-hand mapping"));
            FName Socket(TEXT("hand_r"));
            auto* Fallback = Player->AppearanceComponent->ResolveEquipmentAttachment(Socket, PGGamePlayTags::Weapon_Sword);
            const FTransform FallbackLocal = Fallback->GetRelativeTransform();
            const auto SavedGrips = Appearance->GripProfiles;
            FPGAppearanceGripProfile Grip;
            Grip.WeaponTag = PGGamePlayTags::Weapon_Sword;
            Grip.SourceSocket = TEXT("hand_r"); Grip.TargetSocket = *Hand;
            Grip.GripOffset = FTransform(FRotator(5,10,15),FVector(1,2,3));
            Appearance->GripProfiles = {Grip};
            Socket = Grip.SourceSocket;
            auto* Explicit = Player->AppearanceComponent->ResolveEquipmentAttachment(Socket, Grip.WeaponTag);
            bool bGripOK = Explicit == Fallback && Socket.IsNone() && Explicit->GetRelativeTransform().Equals(Grip.GripOffset);
            Socket = Grip.SourceSocket;
            auto* Other = Player->AppearanceComponent->ResolveEquipmentAttachment(Socket, PGGamePlayTags::Weapon_Bow);
            bGripOK &= Other != Explicit && Other->GetRelativeTransform().Equals(FallbackLocal);
            Appearance->GripProfiles[0].GripOffset.SetTranslation(FVector(4,5,6));
            Socket = Grip.SourceSocket;
            auto* Refreshed = Player->AppearanceComponent->ResolveEquipmentAttachment(Socket, Grip.WeaponTag);
            bGripOK &= Refreshed == Explicit && Refreshed->GetRelativeLocation().Equals(FVector(4,5,6));
            Appearance->GripProfiles[0].TargetSocket = TEXT("PGMissingGripSocket");
            Socket = Grip.SourceSocket;
            auto* Invalid = Player->AppearanceComponent->ResolveEquipmentAttachment(Socket, Grip.WeaponTag);
            bGripOK &= Invalid->GetRelativeTransform().Equals(FallbackLocal);
            Appearance->GripProfiles = {Grip, Grip};
            Socket = Grip.SourceSocket;
            bGripOK &= Player->AppearanceComponent->ResolveEquipmentAttachment(Socket, Grip.WeaponTag)->GetRelativeTransform().Equals(FallbackLocal);
            Appearance->GripProfiles = SavedGrips;
            Socket = Grip.SourceSocket;
            Player->AppearanceComponent->ResolveEquipmentAttachment(Socket, Grip.WeaponTag);
            if (!bGripOK) return Finish(false,TEXT("grip cache/offset/fallback regression"));
            UE_LOG(LogTemp,Display,TEXT("PGCharacterGrip Id=%s TagIsolation=1 Refresh=1 InvalidFallback=1 DuplicateFallback=1"),*Appearance->Id.ToString());
            // Synchronous mesh/rig loads above can exceed the settling interval.
            // Start it after loading so the new anim instance evaluates first.
            State->PhaseAt=FPlatformTime::Seconds(); State->Phase=1; State->Travel=0; State->bCaptured=false;
            return true;
        }
        if (State->Phase==1)
        {
            if (Now-State->PhaseAt<.5) return true;
            auto* Mesh=Player->AppearanceComponent->GetPresentationMesh();
            if (FParse::Param(FCommandLine::Get(),TEXT("PGCharacterCapture")))
            {
                if (!State->bCaptured)
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlayableCharacters")/(Player->AppearanceComponent->GetAppearance()->Id.ToString()+TEXT("_Idle.png")),false,false);
                    State->bCaptured=true;
                }
                if (Now-State->PhaseAt<1.0) return true;
            }
            UE_LOG(LogTemp,Display,TEXT("PGCharacterPose Id=%s Head=%s"),*Player->AppearanceComponent->GetAppearance()->Id.ToString(),
                *Mesh->GetSocketTransform(Player->AppearanceComponent->GetAppearance()->HeadBone,RTS_Component).GetTranslation().ToString());
            State->InitialPose=Mesh->GetComponentSpaceTransforms();
            State->SampleSourceBones.Reset(); State->SampleTargetBones.Reset();
            const auto* Selected = Player->AppearanceComponent->GetAppearance();
            for (const FName Bone : {FName(TEXT("hand_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r")),FName(TEXT("head"))})
                if (const FName* Target = Selected->EquipmentBones.Find(Bone))
                {
                    const int32 SourceIndex = Player->GetMesh()->GetBoneIndex(Bone), TargetIndex = Mesh->GetBoneIndex(*Target);
                    if (SourceIndex != INDEX_NONE && TargetIndex != INDEX_NONE)
                    { State->SampleSourceBones.Add(SourceIndex); State->SampleTargetBones.Add(TargetIndex); }
                }
            const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
            const int32 Head=Ref.FindBoneIndex(Player->AppearanceComponent->GetAppearance()->HeadBone);
            if (Head==INDEX_NONE) return Finish(false,TEXT("missing head bone"));
            FTransform RefHead=Ref.GetRefBonePose()[Head];
            for (int32 Parent=Ref.GetParentIndex(Head);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent)) RefHead*=Ref.GetRefBonePose()[Parent];
            const double ReferenceSpan=FVector::Distance(RefHead.GetTranslation(),Ref.GetRefBonePose()[0].GetTranslation());
            const double ActualSpan=FVector::Distance(State->InitialPose[Head].GetTranslation(),State->InitialPose[0].GetTranslation());
            if (ActualSpan<ReferenceSpan*.5 || ActualSpan>ReferenceSpan*1.5) return Finish(false,TEXT("retargeted body proportions changed"));
            Player->GetSkillHandler()->ResetCombo();
            Player->GetPGAbilitySystemComponent()->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Normal);
            Player->GetPGAbilitySystemComponent()->OnAbilityInputReleased(PGGamePlayTags::InputTag_Skill_Normal);
            if (!Player->GetPlayerAttackComponent()->IsRunning()) return Finish(false,TEXT("GAS normal attack did not start"));
            const FName Other=Catalog[(State->Index+1)%Catalog.Num()].LoadSynchronous()->Id;
            if (Profile->SelectCharacter(Other)) return Finish(false,TEXT("mid-attack selection accepted"));
            State->Phase=2; State->PhaseAt=Now; State->bCaptured=false;
            return true;
        }
        if (State->Phase==2)
        {
            auto* Mesh=Player->AppearanceComponent->GetPresentationMesh();
            const auto& Pose=Mesh->GetComponentSpaceTransforms();
            if (Pose.Num()!=State->InitialPose.Num() || Pose.IsEmpty()) return Finish(false,TEXT("invalid retarget pose"));
            for (int32 Bone=0;Bone<Pose.Num();++Bone)
            {
                if (Pose[Bone].ContainsNaN() || Pose[Bone].GetTranslation().Size()>2000) return Finish(false,TEXT("nonfinite or exploded pose"));
                State->Travel=FMath::Max(State->Travel,static_cast<float>(FVector::Distance(Pose[Bone].GetTranslation(),State->InitialPose[Bone].GetTranslation())));
            }
            const auto& SourcePose = Player->GetMesh()->GetComponentSpaceTransforms();
            for (int32 Sample=0; Sample<State->SampleSourceBones.Num(); ++Sample)
            {
                const int32 SourceIndex = State->SampleSourceBones[Sample], TargetIndex = State->SampleTargetBones[Sample];
                if (!SourcePose.IsValidIndex(SourceIndex) || !Pose.IsValidIndex(TargetIndex)) continue;
                const FVector A = Player->GetMesh()->GetComponentTransform().TransformPosition(SourcePose[SourceIndex].GetTranslation());
                const FTransform TargetWorld = Pose[TargetIndex]*Mesh->GetComponentTransform();
                const FVector B = TargetWorld.GetTranslation(), S = TargetWorld.GetScale3D();
                State->PoseSamples.Add(FString::Printf(TEXT("%s,%.6f,%.6f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f"),
                    *Player->AppearanceComponent->GetAppearance()->Id.ToString(),Now-State->PhaseAt,Player->GetPlayerAttackComponent()->GetLogicalTime(),
                    *Player->GetMesh()->GetBoneName(SourceIndex).ToString(),A.X,A.Y,A.Z,B.X,B.Y,B.Z,S.X,S.Y,S.Z));
            }
            if (!State->bCaptured && Now-State->PhaseAt>.25 && FParse::Param(FCommandLine::Get(),TEXT("PGCharacterCapture")))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlayableCharacters")/(Player->AppearanceComponent->GetAppearance()->Id.ToString()+TEXT(".png")),false,false);
                State->bCaptured=true;
            }
            if (Now-State->PhaseAt<1.5 || Player->GetPlayerAttackComponent()->IsRunning()) return true;
            if (State->Travel<2) return Finish(false,TEXT("visible attack pose stayed static"));
            UE_LOG(LogTemp,Display,TEXT("PGCharacterProbe Player=%s Travel=%.3f SaveAtomic=1 AttackGuard=1"),*Profile->GetProfile()->CharacterId.ToString(),State->Travel);
            if (++State->Index<Catalog.Num()) { State->Phase=0; return true; }
            State->Index=0; State->Phase=3;
        }
        if (State->Phase==3)
        {
            auto* Tables=UPGDataTableManager::Get(W);
            const auto* Row=Tables?Tables->GetRowData<FPGEnemyDataRow>(15201+State->Index):nullptr;
            auto* Class=Row?Row->ActorClass.LoadSynchronous():nullptr;
            if (!Class) return Finish(false,TEXT("missing P09 enemy row/class"));
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy=W->SpawnActor<APGCharacterEnemy>(Class,Player->GetActorLocation()+FVector(300,0,0),FRotator(0,180,0),Params);
            if (!Enemy || !Enemy->AppearanceComponent->GetPresentationMesh() || !Enemy->GetController() || !Enemy->GetSkillHandler())
                return Finish(false,TEXT("P09 appearance/AI/GAS initialization failed"));
            State->Enemy=Enemy; State->Phase=4; State->PhaseAt=Now; State->Travel=0; State->bCaptured=false;
            State->InitialPose=Enemy->AppearanceComponent->GetPresentationMesh()->GetComponentSpaceTransforms();
            return true;
        }
        if (State->Phase==4)
        {
            auto* Enemy=State->Enemy.Get(); if (!Enemy) return Finish(false,TEXT("P09 destroyed unexpectedly"));
            const auto& Pose=Enemy->AppearanceComponent->GetPresentationMesh()->GetComponentSpaceTransforms();
            if (Pose.Num()==State->InitialPose.Num())
                for (int32 Bone=0;Bone<Pose.Num();++Bone)
                {
                    if (Pose[Bone].ContainsNaN() || Pose[Bone].GetTranslation().Size()>2000) return Finish(false,TEXT("invalid or exploded P09 pose"));
                    State->Travel=FMath::Max(State->Travel,static_cast<float>(FVector::Distance(Pose[Bone].GetTranslation(),State->InitialPose[Bone].GetTranslation())));
                }
            if (!State->bCaptured && Now-State->PhaseAt>.6 && FParse::Param(FCommandLine::Get(),TEXT("PGCharacterCapture")))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlayableCharacters")/FString::Printf(TEXT("P09_%d.png"),15201+State->Index),false,false);
                State->bCaptured=true;
            }
            if (Now-State->PhaseAt<2) return true;
            if (State->Travel<1) return Finish(false,TEXT("P09 animation stayed static"));
            auto* Mesh=Enemy->AppearanceComponent->GetPresentationMesh();
            const double BodySpan=FVector::Distance(Mesh->GetSocketLocation(TEXT("Head")),Mesh->GetSocketLocation(TEXT("Hips")));
            if (BodySpan<20 || BodySpan>100) return Finish(false,TEXT("P09 body proportions changed"));
            UE_LOG(LogTemp,Display,TEXT("PGCharacterProbe P09BodySpan=%.3f"),BodySpan);
            UE_LOG(LogTemp,Display,TEXT("PGCharacterProbe Enemy=%d Travel=%.3f AI=1 GAS=1"),15201+State->Index,State->Travel);
            Enemy->Destroy(); State->Enemy.Reset();
            if (++State->Index<4) { State->Phase=3; return true; }
            if (FParse::Param(FCommandLine::Get(),TEXT("PGCharacterCapture")))
            {
                auto* Inventory=CreateWidget<UPGUIInventory>(UGameplayStatics::GetPlayerController(W,0),UPGUIInventory::StaticClass());
                if (!Inventory) return Finish(false,TEXT("character inventory creation failed"));
                Inventory->AddToViewport(100);
                Inventory->SetCharacterTab();
                State->Phase=5; State->PhaseAt=Now; State->bCaptured=false;
                return true;
            }
            return Finish(true,*FString::Printf(TEXT("players=%d monsters=4"),Catalog.Num()));
        }
        if (State->Phase==5 && Now-State->PhaseAt>.5)
        {
            if (!State->bCaptured)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlayableCharacters/CharacterSelection.png"),true,false);
                State->bCaptured=true;
                return true;
            }
            if (Now-State->PhaseAt>1.5) return Finish(true,*FString::Printf(TEXT("players=%d monsters=4"),Catalog.Num()));
        }
        return true;
    }));
}));
#endif

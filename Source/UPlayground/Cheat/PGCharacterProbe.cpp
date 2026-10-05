#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
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
        TWeakObjectPtr<APGCharacterEnemy> Enemy;
    };
    auto State = MakeShared<FState>();
    TWeakObjectPtr<UWorld> WeakWorld = World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,WeakWorld](float)
    {
        auto* W = WeakWorld.Get(); if (!W) return false;
        const auto Finish=[](bool OK,const TCHAR* Reason)
        { UE_LOG(LogTemp,Display,TEXT("PGCharacterProbe %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Reason); FPlatformMisc::RequestExit(false); return false; };
        const double Now = FPlatformTime::Seconds();
        if (Now-State->Started>150) return Finish(false,TEXT("timeout"));
        auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        auto* Profile = UPGProfileSubsystem::Get(W);
        if (!Player || !Profile || !Profile->GetCatalog() || !Player->GetSkillHandler() || Now-State->Started<4) return true;
        const auto& Catalog = Profile->GetCatalog()->PlayableCharacters;
        if (Catalog.Num()!=7) return Finish(false,TEXT("expected seven playable identities"));
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
            State->PhaseAt=Now; State->Phase=1; State->Travel=0; State->bCaptured=false;
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
            const FName Other=Catalog[(State->Index+1)%7].LoadSynchronous()->Id;
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
            if (!State->bCaptured && Now-State->PhaseAt>.25 && FParse::Param(FCommandLine::Get(),TEXT("PGCharacterCapture")))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlayableCharacters")/(Player->AppearanceComponent->GetAppearance()->Id.ToString()+TEXT(".png")),false,false);
                State->bCaptured=true;
            }
            if (Now-State->PhaseAt<1.5 || Player->GetPlayerAttackComponent()->IsRunning()) return true;
            if (State->Travel<2) return Finish(false,TEXT("visible attack pose stayed static"));
            UE_LOG(LogTemp,Display,TEXT("PGCharacterProbe Player=%s Travel=%.3f SaveAtomic=1 AttackGuard=1"),*Profile->GetProfile()->CharacterId.ToString(),State->Travel);
            if (++State->Index<7) { State->Phase=0; return true; }
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
            return Finish(true,TEXT("players=7 monsters=4"));
        }
        if (State->Phase==5 && Now-State->PhaseAt>.5)
        {
            if (!State->bCaptured)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlayableCharacters/CharacterSelection.png"),true,false);
                State->bCaptured=true;
                return true;
            }
            if (Now-State->PhaseAt>1.5) return Finish(true,TEXT("players=7 monsters=4"));
        }
        return true;
    }));
}));
#endif

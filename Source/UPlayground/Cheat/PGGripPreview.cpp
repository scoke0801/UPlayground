#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Rendering/PGCharacterAppearanceComponent.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

// Disposable-profile close-ups and raw pose evidence for authored grip calibration.
static FAutoConsoleCommandWithWorld PGGripPreview(TEXT("PGGripPreview"), TEXT("Capture equipped-hand calibration in a Characters_ test profile."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    FString Profile, Directory;
    if (!World || !FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile) || !Profile.StartsWith(TEXT("Characters_")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("PGGripPreviewOutput="), Directory)) return;
    struct FState
    {
        double Started=FPlatformTime::Seconds(), At=0, CaptureAt=0;
        int32 Phase=0, View=0, Skill=0, Frame=0, Samples=0;
        FTransform WeaponInHand;
        TWeakObjectPtr<ACameraActor> Camera;
    };
    const auto State=MakeShared<FState>();
    TWeakObjectPtr<UWorld> WeakWorld=World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,WeakWorld,Directory](float)
    {
        auto* W=WeakWorld.Get(); if (!W) return false;
        const double Now=FPlatformTime::Seconds();
        const auto Finish=[](bool OK) { UE_LOG(LogTemp,Display,TEXT("PGGripPreview %s"),OK?TEXT("PASS"):TEXT("FAIL")); FPlatformMisc::RequestExit(false); return false; };
        if (Now-State->Started>60) return Finish(false);
        auto* Player=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        auto* Profile=UPGProfileSubsystem::Get(W);
        if (!Player || !Profile || Now-State->Started<4) return true;
        if (State->Phase==0)
        {
            FString Identity=TEXT("Bokusei"); FParse::Value(FCommandLine::Get(),TEXT("PGGripIdentity="),Identity);
            if (!Profile->SelectCharacter(FName(*Identity))) return true;
            if (auto* UI=UPGUIManager::Get(Player)) UI->CloseAllUI();
            auto* Appearance=Player->AppearanceComponent->GetAppearance();
            FString Candidate, Text;
            if (FParse::Value(FCommandLine::Get(),TEXT("PGGripCandidate="),Candidate))
            {
                FPGAppearanceGripProfile Grip;
                if (!FFileHelper::LoadFileToString(Text,*Candidate) || !FPGAppearanceGripProfile::StaticStruct()->ImportText(*Text,&Grip,nullptr,PPF_None,GWarn,TEXT("PGGripPreview"))) return Finish(false);
                // Duplicate the appearance so the candidate cannot alter the loaded data asset.
                auto* Copy=DuplicateObject<UPGCharacterAppearance>(Appearance,Player);
                Copy->GripProfiles={Grip};
                if (!Player->AppearanceComponent->ApplyAppearance(Copy)) return Finish(false);
            }
            auto* Camera=W->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(35);
            State->Camera=Camera;
            UGameplayStatics::GetPlayerController(W,0)->SetViewTarget(Camera);
            State->At=Now; State->Phase=1;
            return true;
        }
        auto* Mesh=Player->AppearanceComponent->GetPresentationMesh();
        auto* Weapon=Player->GetCombatComponent()->GetCharacterCarriedWeaponByTag(PGGamePlayTags::Weapon_Sword);
        if (!Mesh || !Weapon) return Finish(false);
        const FName Hand=Player->AppearanceComponent->GetAppearance()->EquipmentBones.FindRef(TEXT("hand_r"));
        if (State->Phase==1 && Now-State->At>.6)
        {
            TArray<FString> Lines;
            Lines.Add(TEXT("mesh,bone,parent,space,x,y,z,qx,qy,qz,qw,sx,sy,sz"));
            const auto Add=[&](const FString& Label,FName Bone,FName Parent,const TCHAR* Space,const FTransform& T)
            {
                const auto P=T.GetTranslation(); const auto Q=T.GetRotation(); const auto S=T.GetScale3D();
                Lines.Add(FString::Printf(TEXT("%s,%s,%s,%s,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f"),*Label,*Bone.ToString(),*Parent.ToString(),Space,P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z));
            };
            for (auto* Component : {Player->GetMesh(),Mesh})
            {
                const auto& Ref=Component->GetSkeletalMeshAsset()->GetRefSkeleton();
                const auto& Pose=Component->GetComponentSpaceTransforms();
                for (int32 I=0;I<Ref.GetNum();++I)
                {
                    const int32 Parent=Ref.GetParentIndex(I); const FName ParentName=Parent>=0?Ref.GetBoneName(Parent):NAME_None;
                    const FString Label=Component==Mesh?TEXT("target"):TEXT("source");
                    Add(Label,Ref.GetBoneName(I),ParentName,TEXT("ref_local"),Ref.GetRefBonePose()[I]);
                    if (Pose.IsValidIndex(I)) Add(Label,Ref.GetBoneName(I),ParentName,TEXT("world"),Pose[I]*Component->GetComponentTransform());
                }
            }
            Add(TEXT("weapon"),TEXT("root"),NAME_None,TEXT("world"),Weapon->GetActorTransform());
            Add(TEXT("weapon"),TEXT("root"),NAME_None,TEXT("relative"),Weapon->GetRootComponent()->GetRelativeTransform());
            auto* Anchor=Weapon->GetRootComponent()->GetAttachParent();
            Add(TEXT("weapon"),TEXT("anchor"),Anchor->GetAttachSocketName(),TEXT("relative"),Anchor->GetRelativeTransform());
            if (auto* WM=Weapon->GetMeshComponent())
            {
                Add(TEXT("weapon"),TEXT("mesh"),NAME_None,TEXT("relative"),WM->GetRelativeTransform());
                if (auto* Static=Cast<UStaticMeshComponent>(WM)) UE_LOG(LogTemp,Display,TEXT("PGGripPreview WeaponMesh=%s"),*GetPathNameSafe(Static->GetStaticMesh()));
            }
            if (!FFileHelper::SaveStringArrayToFile(Lines,*(Directory/TEXT("pose.csv")))) return Finish(false);
            State->WeaponInHand=Weapon->GetActorTransform().GetRelativeTransform(Mesh->GetSocketTransform(Hand));
            UE_LOG(LogTemp,Display,TEXT("PGGripPreview ActiveGrip=%d"),Player->AppearanceComponent->GetEquippedGripIndex());
            State->Phase=2; State->At=0;
        }
        if (State->Phase==2)
        {
            const FVector Target=Mesh->GetSocketLocation(Hand);
            const FVector Directions[]={FVector(1,-1,.3),FVector(1,1,.3),FVector(0,-1,.1),FVector(1,0,.05)};
            auto* Camera=State->Camera.Get();
            const FVector Offset=Player->GetActorQuat().RotateVector(Directions[State->View].GetSafeNormal()*65);
            Camera->SetActorLocation(Target+Offset);
            Camera->SetActorRotation((-Offset).Rotation());
            if (State->At==0) State->At=Now;
            if (Now-State->At>.6)
            {
                FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("idle_%d.png"),State->View),false,false);
                State->Phase=3; State->At=Now;
            }
        }
        else if (State->Phase==3 && Now-State->At>.25)
        {
            if (++State->View==4)
            {
                if (!FParse::Param(FCommandLine::Get(),TEXT("PGGripMotion"))) return Finish(true);
                State->Phase=4; State->At=Now; return true;
            }
            State->Phase=2; State->At=0;
        }
        else if (State->Phase==4)
        {
            const int32 Skills[]={100,101,102,110,111,112,113,114};
            auto* ASC=Player->GetPGAbilitySystemComponent();
            if (State->Skill<8)
            {
                auto* Handler=Player->GetSkillHandler();
                Handler->RemoveSkill(EPGSkillSlot::SkillSlot_1); Handler->AddSkill(EPGSkillSlot::SkillSlot_1,Skills[State->Skill]);
                auto* Skill=Handler->GetSkillData(EPGSkillSlot::SkillSlot_1);
                Skill->LastSkillUsedTime=-1.e30; Skill->InheritedCooldownUntil=-1.e30;
                ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
                ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_Skill_Slot1);
                if (!Player->GetPlayerAttackComponent()->IsRunning()) return Finish(false);
            }
            else if (State->Skill==8)
            {
                if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(PGGamePlayTags::Player_Ability_Roll))) return Finish(false);
            }
            else if (State->Skill==9)
            {
                // Player input deliberately disallows sheathing. Exercise the same
                // attachment/state transition without changing that gameplay policy.
                FName Socket(TEXT("SwordSocketBack"));
                auto* Attachment=Player->ResolveEquipmentAttachment(Socket,PGGamePlayTags::Weapon_Sword);
                if (!Weapon->AttachToComponent(Attachment,FAttachmentTransformRules::SnapToTargetNotIncludingScale,Socket)) return Finish(false);
                Player->GetCombatComponent()->SetCurrentEquippedWeaponTag(FGameplayTag());
            }
            else if (State->Skill==10)
            {
                FName Socket(TEXT("RightWeaponSocket"));
                auto* Attachment=Player->ResolveEquipmentAttachment(Socket,PGGamePlayTags::Weapon_Sword);
                if (!Weapon->AttachToComponent(Attachment,FAttachmentTransformRules::SnapToTargetNotIncludingScale,Socket)) return Finish(false);
                Player->GetCombatComponent()->SetCurrentEquippedWeaponTag(PGGamePlayTags::Weapon_Sword);
            }
            else return Finish(State->Samples>40);
            State->At=Now; State->CaptureAt=Now; State->Frame=0; State->Phase=5;
        }
        else if (State->Phase==5)
        {
            const FTransform HandWorld=Mesh->GetSocketTransform(Hand);
            const FVector Offset=HandWorld.GetRotation().RotateVector(FVector(1,0,-.5).GetSafeNormal()*70);
            State->Camera->SetActorLocation(HandWorld.GetLocation()+Offset);
            State->Camera->SetActorRotation((-Offset).Rotation());
            const int32 Active=Player->AppearanceComponent->GetEquippedGripIndex();
            if (State->Skill<=8 || (State->Skill==10 && Now-State->At>.4))
            {
                if (Active==INDEX_NONE) return Finish(false);
                const FTransform Relative=Weapon->GetActorTransform().GetRelativeTransform(HandWorld);
                const double Distance=FVector::Distance(Relative.GetLocation(),State->WeaponInHand.GetLocation());
                const double Angle=FMath::RadiansToDegrees(Relative.GetRotation().AngularDistance(State->WeaponInHand.GetRotation()));
                double FingerError=0;
                const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
                const auto& Pose=Mesh->GetBoneSpaceTransforms();
                for (const auto& Finger : Player->AppearanceComponent->GetAppearance()->GripProfiles[Active].Fingers)
                {
                    const int32 Index=Ref.FindBoneIndex(Finger.Bone);
                    if (!Pose.IsValidIndex(Index)) return Finish(false);
                    const FQuat Expected=(Ref.GetRefBonePose()[Index].GetRotation()*Finger.ReferenceRotationOffset.Quaternion()).GetNormalized();
                    FingerError=FMath::Max(FingerError,FMath::RadiansToDegrees(Expected.AngularDistance(Pose[Index].GetRotation())));
                }
                UE_LOG(LogTemp,Display,TEXT("PGGripPreview Sample Motion=%d Distance=%.6f Angle=%.6f Fingers=%.6f"),State->Skill,Distance,Angle,FingerError);
                if (Distance>.1 || Angle>1 || FingerError>1) return Finish(false);
                ++State->Samples;
            }
            if (Now-State->CaptureAt>.2 && State->Frame<6)
            {
                FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("motion_%02d_%02d.png"),State->Skill,State->Frame++),false,false);
                State->CaptureAt=Now;
            }
            if (Now-State->At>2.2 && !Player->GetPlayerAttackComponent()->IsRunning())
            {
                if (State->Skill==9)
                {
                    if (Active!=INDEX_NONE) return Finish(false);
                    const auto* Appearance=Player->AppearanceComponent->GetAppearance();
                    double ReleasedAngle=0;
                    const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
                    for (const auto& Grip : Appearance->GripProfiles)
                        for (const auto& Finger : Grip.Fingers)
                        {
                            const int32 Index=Ref.FindBoneIndex(Finger.Bone);
                            if (Index==INDEX_NONE) return Finish(false);
                            const FQuat Closed=(Ref.GetRefBonePose()[Index].GetRotation()*Finger.ReferenceRotationOffset.Quaternion()).GetNormalized();
                            ReleasedAngle=FMath::Max(ReleasedAngle,FMath::RadiansToDegrees(Closed.AngularDistance(Mesh->GetBoneSpaceTransforms()[Index].GetRotation())));
                        }
                    if (ReleasedAngle<20) return Finish(false);
                    UE_LOG(LogTemp,Display,TEXT("PGGripPreview ReleasedFingerAngle=%.3f"),ReleasedAngle);
                }
                UE_LOG(LogTemp,Display,TEXT("PGGripPreview Motion=%d PASS Active=%d"),State->Skill,Active);
                ++State->Skill; State->Phase=4;
            }
        }
        return true;
    }));
}));
#endif

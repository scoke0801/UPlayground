#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
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
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGUI/Manager/PGUIManager.h"

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
        int32 Index=0;
        bool bReady=false, bPlaced=false, bCaptured=false;
        TArray<int32> Skills={100,101,102,111,112};
        TArray<TWeakObjectPtr<APGCharacterEnemy>> Targets;
        TSharedPtr<FPGSkillCastContext> Context;
        FVector Origin;
    };
    auto State=MakeShared<FProbe>(); TWeakObjectPtr<UWorld> WeakWorld=World;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,WeakWorld](float)
    {
        auto* W=WeakWorld.Get(); if(!W) return false;
        const auto Finish=[](bool bOK,const TCHAR* Reason)
        { UE_LOG(LogTemp,Display,TEXT("PGHackSlashProbe %s %s"),bOK?TEXT("PASS"):TEXT("FAIL"),Reason); FPlatformMisc::RequestExit(false); return false; };
        if(FPlatformTime::Seconds()-State->Started>45) return Finish(false,TEXT("timeout"));
        auto* Player=Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(W,0));
        if(!Player || !Player->GetSkillHandler() || !Player->GetMesh()->GetAnimInstance()) return true;
        auto* ASC=Player->GetPGAbilitySystemComponent(); auto* Attack=Player->GetPlayerAttackComponent();
        auto* Handler=Player->GetSkillHandler();
        if(!State->bReady)
        {
            if(FPlatformTime::Seconds()-State->Started<4) return true;
            FPGStagePresentation View; View.bClose=true;
            if(auto* Messages=UPGMessageManager::Get(Player)) Messages->SendMessage(EPGUIMessageType::StagePresentation,&View);
            if(auto* UI=UPGUIManager::Get(Player)) UI->CloseAllUI();
            if(!Player->IsGameplayInputAllowed()) return true;
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
        if(!State->Context)
        {
            if(State->Index>=State->Skills.Num()) return Finish(true,TEXT("skills=5 real_spatial=1 contact_injected=0 source_attack=100"));
            Player->SetActorLocation(State->Origin,false,nullptr,ETeleportType::TeleportPhysics);
            Player->GetCharacterMovement()->StopMovementImmediately();
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Handler->RemoveSkill(EPGSkillSlot::SkillSlot_1); Handler->AddSkill(EPGSkillSlot::SkillSlot_1,State->Skills[State->Index]);
            ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
            State->Context=Attack->GetCastContext();
            if(!State->Context) return Finish(false,TEXT("profile cast did not commit"));
            State->CastAt=FPlatformTime::Seconds(); State->bPlaced=false; State->bCaptured=false;
            return true;
        }
        if(!State->bPlaced && Attack->GetLogicalTime()>=.1f)
        {
            const int32 Skill=State->Skills[State->Index];
            const FVector F=Player->GetActorForwardVector(), R=Player->GetActorRightVector();
            for(int32 Index=0;Index<2;++Index)
            {
                const FVector Offset=Skill==111 ? F*(Index?550.f:220.f)+R*110.f : F*(Index?-150.f:150.f);
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
        const int32 CaptureSkill=State->Skills[State->Index];
        const float FirstHit=CaptureSkill==102?.30f:CaptureSkill==101?.19f:CaptureSkill==111?.18f:CaptureSkill==112?.20f:.16f;
        if(!State->bCaptured && Attack->GetLogicalTime()>=FirstHit+.02f && State->Context->HitTargets.Num()>0 && FParse::Param(FCommandLine::Get(),TEXT("PGHackSlashCapture")))
        {
            State->bCaptured=true;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("QA/HackSlashP0")/
                FString::Printf(TEXT("Skill_%d.png"),State->Skills[State->Index]),false,false);
        }
        if(!Attack->IsRunning())
        {
            if(!State->bPlaced) return Finish(false,TEXT("cast ended before first phase"));
            const int32 Skill=State->Skills[State->Index];
            const float Power=Skill==100?90.f:Skill==102?150.f:Skill==111?90.f:Skill==112?200.f:100.f;
            for(int32 Index=0;Index<State->Targets.Num();++Index)
            {
                auto* Target=State->Targets[Index].Get(); if(!Target) return Finish(false,TEXT("target destroyed"));
                const float Damage=10000-Target->GetPGAbilitySystemComponent()->GetHealth();
                const float Expected=Index==1 && Skill<110?0.f:Power;
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

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include <limits>
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Components/Combat/PGPlayerSkillProjectile.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimComposite.h"

namespace
{
UPGPlayerSkillProfile* TestProfile()
{
    auto* Profile = NewObject<UPGPlayerSkillProfile>();
    Profile->SkillID = 112; Profile->Duration = .76f;
    Profile->HitPhases.SetNum(2);
    for (int32 Index=0; Index<2; ++Index)
    {
        auto& Hit = Profile->HitPhases[Index]; Hit.PhaseId=Index;
        Hit.Start = Index ? .48f : .20f; Hit.End=Hit.Start+.06f;
        Hit.Shape=EPGPlayerHitShape::Disc; Hit.Radius=320; Hit.DamageMultiplier=1; Hit.HitStopSeconds=0;
        Hit.ProcPolicy.bBleedBurst = Index == 1;
    }
    Profile->PoseKeys.SetNum(2); Profile->PoseKeys[1].Time=.76f; Profile->PoseKeys[1].MontageSeconds=1.f;
    return Profile;
}
UWorld* TestWorld()
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
}
void Stats(APGCharacterBase* Character, int32 HP=10000)
{
    auto* ASC=Character->GetPGAbilitySystemComponent(); ASC->InitAbilityActorInfo(Character,Character);
    ASC->InitializeCombatStats({{EPGStatType::Health,HP},{EPGStatType::Attack,100},{EPGStatType::Defense,0},{EPGStatType::CriticalRate,0}});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerMotionSwingTest,"PG.HackSlash.MotionSwingCues",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGPlayerMotionSwingTest::RunTest(const FString&)
{
    auto* Profile = TestProfile();
    auto* Montage = NewObject<UAnimMontage>();
    Montage->SlotAnimTracks.Reset();
    auto* Sequence = NewObject<UAnimComposite>();
    for (float Time : {.1f, .3f, .5f, .7f, .9f})
    {
        auto& Notify = Sequence->Notifies.AddDefaulted_GetRef();
        Notify.NotifyName = TEXT("P_HitPoint"); Notify.SetTime(Time);
    }
    FAnimSegment Segment; Segment.SetAnimReference(Sequence);
    Segment.AnimStartTime = .2f; Segment.AnimEndTime = .8f;
    Segment.StartPos = .1f; Segment.AnimPlayRate = 2.f; Segment.LoopingCount = 2;
    Montage->SlotAnimTracks.AddDefaulted_GetRef().AnimTrack.AnimSegments.Add(Segment);
    // Duplicate slots must not double-spawn. Three trimmed contacts x two loops = six cues.
    const auto DuplicateSlot = Montage->SlotAnimTracks[0];
    Montage->SlotAnimTracks.Add(DuplicateSlot);
    const auto Cues = UPGPlayerAttackComponent::BuildSwingCues(Profile, Montage);
    TestEqual(TEXT("All six motion strokes have FX"), Cues.Num(), 6);
    const auto Hits = Profile->ResolveHitPhases(Montage);
    TestEqual(TEXT("Every source stroke has a damage phase"), Hits.Num(), 6);
    TestEqual(TEXT("Saved damage templates remain unchanged"), Profile->HitPhases.Num(), 2);
    for (int32 Index = 0; Index < Cues.Num(); ++Index)
    {
        TestEqual(TEXT("Trim, offset, playback rate and loop mapping"), Cues[Index].MontageSeconds, .15f + Index * .1f, .0001f);
        TestEqual(TEXT("Presentation agrees with sampled pose clock"), Profile->GetMontagePosition(Cues[Index].Time), Cues[Index].MontageSeconds, .0001f);
        TestEqual(TEXT("Damage and Niagara start together"), Hits[Index].Start, Cues[Index].Time);
        TestEqual(TEXT("Each stroke has an independent damage ID"), Hits[Index].PhaseId, Index);
        TestEqual(TEXT("Each stroke retains its template damage"), Hits[Index].DamageMultiplier, 1.f);
    }
    Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimPlayRate = -2.f;
    Montage->SlotAnimTracks.SetNum(1);
    TestEqual(TEXT("Reverse playback retains all contacts"), UPGPlayerAttackComponent::BuildSwingCues(Profile, Montage).Num(), 6);
    Profile->SwingNotifyName = NAME_None;
    const auto Fallback = UPGPlayerAttackComponent::BuildSwingCues(Profile, Montage);
    TestEqual(TEXT("Unmarked motions retain authored phase FX"), Fallback.Num(), 2);
    TestEqual(TEXT("Fallback time unchanged"), Fallback[0].Time, Profile->HitPhases[0].Start);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerProfileDataTest,"PG.HackSlash.ProfileValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGPlayerProfileDataTest::RunTest(const FString&)
{
    auto* Profile=TestProfile(); FString Error;
    TestTrue(TEXT("Valid player profile independent of enemy patterns"),Profile->Validate(112,Error));
    TestFalse(TEXT("Reject wrong skill binding"),Profile->Validate(111,Error));
    Profile->HitPhases[1].PhaseId=0; TestFalse(TEXT("Reject duplicate phases"),Profile->Validate(112,Error));
    Profile->HitPhases[1].PhaseId=1;
    Profile->HitPhases[0].MovementSegmentId=TEXT("missing"); TestFalse(TEXT("Reject missing movement"),Profile->Validate(112,Error));
    Profile->HitPhases[0].MovementSegmentId=NAME_None;
    Profile->DodgeCancel=-1.f; TestFalse(TEXT("Reject negative cancel"),Profile->Validate(112,Error)); Profile->DodgeCancel=.28f;
    Profile->HitPhases[0].Radius=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Reject NaN"),Profile->Validate(112,Error));
    Profile->HitPhases[0].Radius=320;
    Profile->PoseKeys[1].MontageSeconds=0; TestFalse(TEXT("Reject nonmonotonic pose mapping"),Profile->Validate(112,Error));
    FPGPlayerHitPhase Fan; Fan.Radius=250; Fan.FullAngleDegrees=120;
    const auto Contains=[&Fan](FVector Point,float Radius=0.f){return UPGPlayerAttackComponent::ContainsTarget(Fan,FVector::ZeroVector,FVector::ForwardVector,Point,Radius);};
    TestTrue(TEXT("Radius 1cm inside"),Contains(FVector(249,0,0)));
    TestFalse(TEXT("Radius 1cm outside"),Contains(FVector(251,0,0)));
    TestFalse(TEXT("Behind player excluded"),Contains(FVector(-100,0,0)));
    TestFalse(TEXT("Different floor excluded"),Contains(FVector(100,0,151)));
    TestTrue(TEXT("Capsule footprint overlaps rim"),Contains(FVector(260,0,0),20));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerPoseContinuityTest,"PG.HackSlash.PoseContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGPlayerPoseContinuityTest::RunTest(const FString&)
{
    auto* Profile=TestProfile();
    const float Times[]={0.f,.16f,.22f,.34f,.40f,.62f,.68f,.92f};
    const float Poses[]={0.f,.171f,.651f,.96f,1.44f,1.86f,2.406f,4.05f};
    Profile->PoseKeys.SetNum(UE_ARRAY_COUNT(Times));
    for(int32 Index=0;Index<Profile->PoseKeys.Num();++Index)
    {
        Profile->PoseKeys[Index].Time=Times[Index];
        Profile->PoseKeys[Index].MontageSeconds=Poses[Index];
    }
    for(int32 Index=0;Index<Profile->PoseKeys.Num();++Index)
    {
        TestEqual(TEXT("Authored pose/hit alignment is unchanged"),Profile->GetMontagePosition(Times[Index]),Poses[Index],.00001f);
        if(Index>0 && Index<Profile->PoseKeys.Num()-1)
        {
            const float Epsilon=.00001f;
            const float Left=(Profile->GetMontagePosition(Times[Index])-Profile->GetMontagePosition(Times[Index]-Epsilon))/Epsilon;
            const float Right=(Profile->GetMontagePosition(Times[Index]+Epsilon)-Profile->GetMontagePosition(Times[Index]))/Epsilon;
            TestEqual(TEXT("No playback velocity jump at a pose key"),Left,Right,.08f);
        }
    }
    float Previous=Profile->GetMontagePosition(-1.f);
    TestEqual(TEXT("Before start holds first pose"),Previous,0.f);
    for(int32 Step=1;Step<=920;++Step)
    {
        const float Position=Profile->GetMontagePosition(Step*.001f);
        TestTrue(TEXT("Continuous samples remain finite, monotone and within montage"),
            FMath::IsFinite(Position) && Position>=Previous && Position<=4.05001f);
        Previous=Position;
    }
    TestEqual(TEXT("After end holds final pose"),Profile->GetMontagePosition(2.f),4.05f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerProfileProcTest,"PG.HackSlash.CastDamageAndProcLimits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGPlayerProfileProcTest::RunTest(const FString&)
{
    auto* World=TestWorld(); auto* Player=World->SpawnActor<APGCharacterPlayer>(); Stats(Player);
    auto* ASC=Player->GetPGAbilitySystemComponent();
    auto Context=MakeShared<FPGSkillCastContext>(); Context->Caster=Player; Context->SkillID=111; Context->Attack=100;
    FPGHitProcPolicy Policy;
    auto* Enemy=World->SpawnActor<APGCharacterEnemy>(); Stats(Enemy);
    auto* Target=Enemy->GetPGAbilitySystemComponent();
    for(int32 Repeat=0;Repeat<10;++Repeat) ASC->ApplyPlayerProfileHit(Enemy,Context,0,.9f,false,Policy,0);
    TestEqual(TEXT("Ten repeated events only deal one phase"),Target->GetHealth(),9910.f);
    ASC->ApplyStatBonus(EPGStatType::Attack,900);
    ASC->ApplyPlayerProfileHit(Enemy,Context,1,.9f,false,Policy,0);
    TestEqual(TEXT("Second phase uses committed attack snapshot"),Target->GetHealth(),9820.f);
    Context=MakeShared<FPGSkillCastContext>(); Context->Caster=Player; Context->SkillID=100; Context->Attack=100;
    ASC->SetCombatPerks({{EPGCombatPerk::Frenzy,3},{EPGCombatPerk::Shockwave,10}});
    for(int32 Index=0;Index<15;++Index)
    {
        auto* Other=World->SpawnActor<APGCharacterEnemy>(); Stats(Other);
        ASC->ApplyPlayerProfileHit(Other,Context,0,1.f,false,Policy,0);
        ASC->ApplyPlayerProfileHit(Other,Context,1,1.f,false,Policy,0);
    }
    TestEqual(TEXT("Fifteen targets and two phases share +3 frenzy cap"),Context->FrenzyGranted,3);
    TestEqual(TEXT("ASC stacks respect cast cap"),ASC->GetBuildCombatState().FrenzyStacks,3);
    TestTrue(TEXT("Basic attack has explicit shock eligibility"),Context->bShockUsed);
    ASC->SetCombatPerks({{EPGCombatPerk::Bleed,10},{EPGCombatPerk::BleedBurst,100}});
    auto Burst=MakeShared<FPGSkillCastContext>(); Burst->Caster=Player; Burst->SkillID=112; Burst->Attack=100;
    auto* BleedTarget=World->SpawnActor<APGCharacterEnemy>(); Stats(BleedTarget);
    ASC->ApplyPlayerProfileHit(BleedTarget,Burst,0,1.f,false,Policy,0);
    TestEqual(TEXT("Non-final hit adds one bleed"),ASC->GetBuildCombatState().BleedStacks,1);
    Policy.bBleedBurst=true;
    ASC->ApplyPlayerProfileHit(BleedTarget,Burst,1,1.f,false,Policy,0);
    TestTrue(TEXT("Final hit claims per-target burst"),Burst->BurstTargets.Contains(BleedTarget));
    TestEqual(TEXT("Burst consumes bleed after adding current hit"),ASC->GetBuildCombatState().BleedStacks,0);
    auto* Dying=World->SpawnActor<APGCharacterEnemy>(); Stats(Dying,10);
    ASC->ApplyPlayerProfileHit(Dying,Burst,1,1.f,false,Policy,0);
    TestFalse(TEXT("Direct kill cannot initiate burst"),Burst->BurstTargets.Contains(Dying));
    auto* PC=World->SpawnActor<APlayerController>(); PC->Possess(Player);
    auto* Handler=Player->GetSkillHandler();
    if (TestNotNull(TEXT("Refund test has a real handler"),Handler))
    {
        Handler->AddSkill(EPGSkillSlot::SkillSlot_1,0);
        auto* Skill=Handler->GetSkillData(EPGSkillSlot::SkillSlot_1);
        Skill->SkillId=111; Skill->CoolTime=10; Skill->LastSkillUsedTime=Skill->GetTime();
        ASC->BeginCombatSkill(EPGSkillSlot::SkillSlot_2); // Deliberately different from the original skill.
        ASC->SetCombatPerks({{EPGCombatPerk::Bleed,10},{EPGCombatPerk::BleedBurst,100},{EPGCombatPerk::BleedRecast,1}});
        auto Refund=MakeShared<FPGSkillCastContext>(); Refund->Caster=Player; Refund->SkillID=111; Refund->Attack=100; Refund->bRefundEligible=true;
        for(int32 Index=0;Index<2;++Index)
        {
            auto* Victim=World->SpawnActor<APGCharacterEnemy>(); Stats(Victim,105);
            ASC->ApplyPlayerProfileHit(Victim,Refund,1,1.f,false,Policy,0);
        }
        TestTrue(TEXT("Cast claims refund before additional burst kills"),Refund->bRefundUsed);
        TestEqual(TEXT("Original SkillID cooldown refunded 35 percent exactly once"),Skill->GetRemainingCooldown(),6.5f,.001f);
    }
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGSkillObservationTest,"PG.HackSlash.CastObservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGSkillObservationTest::RunTest(const FString&)
{
    auto* Observe = IConsoleManager::Get().FindConsoleVariable(TEXT("pg.Skill.Observe"));
    if (!TestNotNull(TEXT("Optional observation switch"), Observe)) return false;
    const int32 Previous = Observe->GetInt(); Observe->Set(1, ECVF_SetByCode);
    auto* World = TestWorld();
    auto* Player = World->SpawnActor<APGCharacterPlayer>(); Stats(Player);
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>(); Stats(Enemy);
    auto* ASC = Player->GetPGAbilitySystemComponent();
    ASC->SetCombatPerks({{EPGCombatPerk::Bleed,10},{EPGCombatPerk::BleedBurst,100}});
    auto Context = MakeShared<FPGSkillCastContext>();
    Context->Caster = Player; Context->SkillID = 112; Context->Attack = 100;
    auto Observation = ASC->BeginSkillObservation(112, Context);
    TestTrue(TEXT("Observation shares committed identity"), Observation->CastId == Context->CastId);
    TestEqual(TEXT("Programmatic cast is not a fresh input"), Observation->InputAt, -1.);
    FPGHitProcPolicy Policy;
    ASC->ApplyPlayerProfileHit(Enemy,Context,0,1.f,false,Policy,0);
    ASC->ApplyPlayerProfileHit(Enemy,Context,0,1.f,false,Policy,0);
    Policy.bBleedBurst = true;
    ASC->ApplyPlayerProfileHit(Enemy,Context,1,1.f,false,Policy,0);
    TestEqual(TEXT("Duplicate target is not an observed hit"),Observation->Hits,2);
    TestEqual(TEXT("Only direct GAS damage is attributed"),Observation->DirectDamage,200.f);
    TestTrue(TEXT("Secondary burst remains outside direct total"),Enemy->GetPGAbilitySystemComponent()->GetHealth()<9800.f);
    ASC->EndSkillObservation(Observation,false);
    ASC->EndSkillObservation(Observation,true);
    TestTrue(TEXT("Cast end is idempotent"),Observation->bEnded);
    TestEqual(TEXT("Completed cast no longer receives incoming attribution"),ASC->GetObservedCastId(),FString(TEXT("none")));
    Observe->Set(0,ECVF_SetByCode);
    TestFalse(TEXT("Disabled observation allocates no cast state"),ASC->BeginSkillObservation(112,Context).IsValid());
    World->DestroyWorld(false); Observe->Set(Previous,ECVF_SetByCode);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerProfileLifecycleTest,"PG.HackSlash.SpatialClockAndCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGPlayerProfileLifecycleTest::RunTest(const FString&)
{
    auto* World=TestWorld(); auto* Player=World->SpawnActor<APGCharacterPlayer>(); Stats(Player);
    Player->SetActorLocation(FVector(0,0,96));
    auto* Attack=Player->GetPlayerAttackComponent();
    auto* Enemy=World->SpawnActor<APGCharacterEnemy>(); Stats(Enemy);
    Enemy->SetActorLocation(FVector(150,0,Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    Enemy->GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel1);
    Enemy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    const auto Begin=[&]()
    {
        Attack->ActiveProfile=TestProfile(); Attack->LogicalTime=0; Attack->PresentedPhases.Reset();
        Attack->CastContext=MakeShared<FPGSkillCastContext>(); Attack->CastContext->Caster=Player;
        Attack->CastContext->SkillID=112; Attack->CastContext->Attack=100;
        Attack->SavedWalkSpeed=600; Attack->LockedForward=FVector::ForwardVector; Attack->bAimLocked=true;
        Attack->SwingCues=UPGPlayerAttackComponent::BuildSwingCues(Attack->ActiveProfile,nullptr);
        Attack->NextSwingCue=0;
    };
    Begin(); auto Context=Attack->CastContext;
    auto* Montage=NewObject<UAnimMontage>();
    for (float Time : {0.f,.35f,.48f})
    {
        auto& Notify=Montage->Notifies.AddDefaulted_GetRef();
        Notify.NotifyName=TEXT("P_HitPoint"); Notify.SetTime(Attack->ActiveProfile->GetMontagePosition(Time));
    }
    Attack->ActiveProfile->HitPhases=Attack->ActiveProfile->ResolveHitPhases(Montage);
    Attack->ActiveProfile->HitPhases[0].End=Attack->ActiveProfile->HitPhases[0].Start;
    Attack->SwingCues=UPGPlayerAttackComponent::BuildSwingCues(Attack->ActiveProfile,nullptr);
    Attack->Advance(.6f);
    TestEqual(TEXT("600ms hitch presents every motion stroke once"),Attack->GetPresentedSwingCount(),3);
    TestEqual(TEXT("Every stroke deals damage once, including the middle thrust"),Enemy->GetPGAbilitySystemComponent()->GetHealth(),9700.f);
    TestEqual(TEXT("Three separate target sets"),Context->HitTargets.Num(),3);
    Attack->Stop(); Begin();
    Attack->Advance(.27f); Attack->Stop(); Attack->Advance(.5f);
    TestTrue(TEXT("Cancellation clears pending motion FX"),Attack->GetSwingCues().IsEmpty());
    TestEqual(TEXT("Cancellation eliminates future hit"),Enemy->GetPGAbilitySystemComponent()->GetHealth(),9600.f);
    TestEqual(TEXT("Movement speed restored"),Player->GetCharacterMovement()->MaxWalkSpeed,600.f);
    TestFalse(TEXT("No active tick after stop"),Attack->IsComponentTickEnabled());
    auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(10,100,150));
    Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Wall->SetActorLocation(FVector(75,0,50));
    Begin(); Attack->Advance(.6f);
    TestEqual(TEXT("World wall occludes disc hits"),Enemy->GetPGAbilitySystemComponent()->GetHealth(),9600.f);
    Attack->Stop(); Wall->Destroy(); Enemy->Destroy();
    auto* Floor=World->SpawnActor<AActor>(); auto* Ground=NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(Ground); Ground->SetBoxExtent(FVector(2000,2000,50));
    Ground->SetCollisionObjectType(ECC_WorldStatic); Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionResponseToAllChannels(ECR_Block); Ground->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-50));
    for(float Rate : {.75f,1.f,1.75f})
    {
        Player->SetActorLocation(FVector(0,0,98)); Begin(); // CharacterMovement normally maintains a small floor gap.
        FPGPlayerMovementSegment Move; Move.SegmentId=TEXT("dash"); Move.Start=.1f; Move.End=.44f; Move.Distance=450; Move.bEndCastOnBlock=true;
        Attack->ActiveProfile->MovementSegments={Move};
        for(int32 Frame=0;Frame<120 && Attack->IsRunning();++Frame) Attack->Advance(Rate/60.f);
        TestEqual(FString::Printf(TEXT("Dash distance at speed %.2f"),Rate),Player->GetActorLocation().X,450.,.1);
        TestFalse(TEXT("Normal completion releases cast"),Attack->IsRunning());
    }
    Player->SetActorLocation(FVector(0,0,98)); Begin();
    FPGPlayerMovementSegment Dash; Dash.SegmentId=TEXT("dash"); Dash.Start=.1f; Dash.End=.44f; Dash.Distance=450; Dash.bEndCastOnBlock=true;
    Attack->ActiveProfile->MovementSegments={Dash};
    auto* Obstacle=World->SpawnActor<AActor>(); auto* Block=NewObject<UBoxComponent>(Obstacle);
    Obstacle->SetRootComponent(Block); Block->SetBoxExtent(FVector(10,100,150));
    Block->SetCollisionObjectType(ECC_WorldStatic); Block->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Block->SetCollisionResponseToAllChannels(ECR_Block); Block->RegisterComponent(); Obstacle->SetActorLocation(FVector(200,0,100));
    Attack->Advance(.6f);
    TestFalse(TEXT("Blocked dash cancels remaining phases"),Attack->IsRunning());
    TestTrue(TEXT("Capsule stops before wall"),Player->GetActorLocation().X < 190.f);
    Obstacle->Destroy();
    auto* BodyTarget=World->SpawnActor<APGCharacterEnemy>(); Stats(BodyTarget);
    BodyTarget->SetActorLocation(FVector(200,0,98));
    BodyTarget->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    Player->SetActorLocation(FVector(0,0,98)); Begin(); Attack->ActiveProfile->MovementSegments={Dash};
    Attack->Advance(.6f);
    TestFalse(TEXT("Overlap-only enemy body still stops dash"),Attack->IsRunning());
    TestTrue(TEXT("Elite/boss safety does not rely on legacy capsule response"),Player->GetActorLocation().X < 170.f);
    Attack->Stop(); BodyTarget->Destroy(); Player->SetActorLocation(FVector(0,0,98)); Begin();
    auto* LandingTarget=World->SpawnActor<APGCharacterEnemy>(); Stats(LandingTarget);
    LandingTarget->SetActorLocation(FVector(800,0,98));
    LandingTarget->GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel1);
    LandingTarget->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    auto* Leap=Attack->ActiveProfile.Get(); Leap->SkillID=110; Leap->Duration=.9f; Leap->AimLock=.12f;
    Leap->EarlyDodgeUntil=.12f; Leap->DodgeCancel=.62f; Leap->AttackCancel=.72f;
    Leap->HitPhases.SetNum(1); Leap->HitPhases[0].Start=.52f; Leap->HitPhases[0].End=.52f;
    Leap->HitPhases[0].Radius=300; Leap->HitPhases[0].DamageMultiplier=2.2f;
    FPGPlayerMovementSegment Jump; Jump.SegmentId=TEXT("leap"); Jump.Mode=EPGPlayerMoveMode::GroundLeap;
    Jump.Start=.12f; Jump.End=.52f; Jump.Distance=600; Jump.bEndCastOnBlock=true; Leap->MovementSegments={Jump};
    TestTrue(TEXT("Leap preflight finds safe route"),Attack->FindLeapDistance(Leap,FVector::ForwardVector,Attack->LeapDistance));
    TestEqual(TEXT("Full valid leap distance"),Attack->LeapDistance,600.f,.1f);
    TestTrue(TEXT("Dodge before takeoff allowed"),Attack->CanCancel(true));
    Attack->Advance(.2f); TestFalse(TEXT("Airborne dodge rejected"),Attack->CanCancel(true));
    auto LeapContext=Attack->CastContext; Attack->Advance(.4f);
    TestEqual(TEXT("Landing performs one spatial query"),LeapContext->SpatialQueries,1);
    TestEqual(TEXT("Landing disc deals 220 at destination"),LandingTarget->GetPGAbilitySystemComponent()->GetHealth(),9780.f);
    TestEqual(TEXT("Leap ends at safe destination"),Player->GetActorLocation().X,600.,.1);
    Attack->Advance(.03f); TestTrue(TEXT("Dodge after recovery threshold allowed"),Attack->CanCancel(true));
    Attack->Stop();
    World->DestroyWorld(false); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerP1ProjectileTest,"PG.HackSlash.ProjectileLifetimeAndSweep",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGPlayerP1ProjectileTest::RunTest(const FString&)
{
    auto* World=TestWorld(); auto* Player=World->SpawnActor<APGCharacterPlayer>(); Stats(Player);
    auto* ASC=Player->GetPGAbilitySystemComponent();
    auto* Profile=TestProfile(); Profile->SkillID=114;
    FPGPlayerHitPhase Phase; Phase.Shape=EPGPlayerHitShape::Projectile; Phase.Radius=80; Phase.DamageMultiplier=1.6f; Phase.HitStopSeconds=0;
    TArray<APGCharacterEnemy*> Targets;
    for (FVector Position : {FVector(30,0,96),FVector(450,0,96),FVector(900,0,96),FVector(450,0,500)})
    {
        auto* Enemy=World->SpawnActor<APGCharacterEnemy>(); Stats(Enemy); Enemy->SetActorLocation(Position);
        Enemy->GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel1);
        Enemy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Targets.Add(Enemy);
    }
    auto Context=MakeShared<FPGSkillCastContext>(); Context->Caster=Player; Context->SkillID=114; Context->Attack=100;
    auto* Projectile=World->SpawnActor<APGPlayerSkillProjectile>(FVector(0,0,80),FRotator::ZeroRotator);
    Projectile->Initialize(Profile,Phase,Context,FVector::ForwardVector);
    TestEqual(TEXT("Spawn overlap deals exactly one 160 hit"),Targets[0]->GetPGAbilitySystemComponent()->GetHealth(),9840.f);
    ASC->BeginCombatSkill(EPGSkillSlot::SkillSlot_1); ASC->ApplyStatBonus(EPGStatType::Attack,900);
    Projectile->Tick(.6f); // One 600ms frame must cover the complete path.
    TestEqual(TEXT("Spawn target never double hits"),Targets[0]->GetPGAbilitySystemComponent()->GetHealth(),9840.f);
    TestEqual(TEXT("Long frame hits middle target with original snapshot"),Targets[1]->GetPGAbilitySystemComponent()->GetHealth(),9840.f);
    TestEqual(TEXT("Long frame hits far target"),Targets[2]->GetPGAbilitySystemComponent()->GetHealth(),9840.f);
    TestEqual(TEXT("Other floor excluded"),Targets[3]->GetPGAbilitySystemComponent()->GetHealth(),10000.f);
    TestTrue(TEXT("Range expires projectile"),Projectile->IsActorBeingDestroyed());
    TestEqual(TEXT("Original skill identity preserved"),Context->SkillID,114);
    auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10,150,150)); Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent(); Wall->SetActorLocation(FVector(200,0,100));
    Context=MakeShared<FPGSkillCastContext>(); Context->Caster=Player; Context->SkillID=114; Context->Attack=100;
    Projectile=World->SpawnActor<APGPlayerSkillProjectile>(FVector(0,0,80),FRotator::ZeroRotator);
    Projectile->Initialize(Profile,Phase,Context,FVector::ForwardVector); Projectile->Tick(.6f);
    TestEqual(TEXT("Wall prevents far damage"),Targets[1]->GetPGAbilitySystemComponent()->GetHealth(),9840.f);
    TestTrue(TEXT("Wall destroys projectile"),Projectile->IsActorBeingDestroyed());
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerP1CooldownTest,"PG.HackSlash.CooldownIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGPlayerP1CooldownTest::RunTest(const FString&)
{
    auto* World=TestWorld(); auto* Player=World->SpawnActor<APGCharacterPlayer>(); Stats(Player);
    FPGSkillHandler Handler; Handler.SetContext(Player);
    Handler.AddSkill(EPGSkillSlot::SkillSlot_1,0);
    auto* A=Handler.GetSkillData(EPGSkillSlot::SkillSlot_1); A->SkillId=110; A->CoolTime=5; A->LastSkillUsedTime=A->GetTime();
    Handler.RemoveSkill(EPGSkillSlot::SkillSlot_1);
    Handler.AddSkill(EPGSkillSlot::SkillSlot_1,0);
    auto* B=Handler.GetSkillData(EPGSkillSlot::SkillSlot_1); B->SkillId=114; B->CoolTime=3;
    TestEqual(TEXT("Different skill does not inherit previous slot cooldown"),B->GetRemainingCooldown(),0.f);
    TestEqual(TEXT("Original unequipped ID can refund"),Handler.RefundRemainingCooldownByID(110,.35f),1.75f,.001f);
    Handler.RemoveSkill(EPGSkillSlot::SkillSlot_1); Handler.AddSkill(EPGSkillSlot::SkillSlot_2,110);
    A=Handler.GetSkillData(EPGSkillSlot::SkillSlot_2); A->SkillId=110; A->CoolTime=5;
    TestEqual(TEXT("Reequipping in another slot retains remaining time"),A->GetRemainingCooldown(),3.25f,.001f);
    TestTrue(TEXT("Reequipped cooldown blocks use"),A->IsOnCooldown());
    World->DestroyWorld(false); return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGData/DataAsset/Input/PGQuarterViewData.h"
#include "PGData/DataAsset/Input/PGCameraSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCameraModesTest, "PG.Camera.ModesAndLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCameraModesTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* Data = NewObject<UPGQuarterViewData>(Player);
    Player->QuarterViewData = Data;
    Player->ConfigureQuarterView();
    Player->SetCameraMode(EPGCameraMode::QuarterView);
    Player->TargetCameraDistance = 950.f;
    Player->CameraPitchOffset = 7.f;
    Player->CameraBoom->TargetArmLength = 950.f;
    Player->UpdatePlayerCamera(0.f);
    const FRotator QuarterRotation = Player->CameraBoom->GetComponentRotation();
    TestTrue(TEXT("Quarter-view yaw is fixed"), FMath::IsNearlyEqual(QuarterRotation.Yaw, Data->Rotation.Yaw));
    Player->SetActorRotation(FRotator(0.f, 135.f, 0.f));
    Player->UpdatePlayerCamera(0.f);
    TestTrue(TEXT("Character turning does not rotate the camera"), Player->CameraBoom->GetComponentRotation().Equals(QuarterRotation));

    Player->SetCameraMode(EPGCameraMode::Action3D);
    TestTrue(TEXT("Action mode uses its own distance"), FMath::IsNearlyEqual(Player->TargetCameraDistance, Data->ActionDistance));
    Player->ActionCameraRotation = FRotator::ZeroRotator;
    Player->ApplyActionCameraMouseDelta(10.f, 10.f);
    TestTrue(TEXT("Positive raw mouse Y now looks up"), Player->ActionCameraRotation.Pitch > 0.f);
    TestTrue(TEXT("Horizontal mouse direction is preserved"), Player->ActionCameraRotation.Yaw > 0.f);
    Player->ApplyActionCameraMouseDelta(-10.f, -10.f);
    TestTrue(TEXT("Opposite displacement restores view"), Player->ActionCameraRotation.IsNearlyZero());
    Player->ActionCameraRotation = FRotator(40.f, 123.f, 20.f);
    Player->UpdatePlayerCamera(0.f);
    TestTrue(TEXT("Action camera supports yaw and upward pitch, without roll"), Player->CameraBoom->GetComponentRotation().Equals(FRotator(40.f, 123.f, 0.f), .01f));
    Player->ActionCameraRotation.Pitch = 120.f;
    Player->UpdatePlayerCamera(0.f);
    TestTrue(TEXT("Action pitch cannot flip over"), FMath::IsNearlyEqual(Player->ActionCameraRotation.Pitch, Data->ActionMaxPitch));
    Player->ActionCameraRotation.Pitch = -120.f;
    Player->UpdatePlayerCamera(0.f);
    TestTrue(TEXT("Action pitch respects lower limit"), FMath::IsNearlyEqual(Player->ActionCameraRotation.Pitch, Data->ActionMinPitch));
    Player->TargetCameraDistance = 350.f;
    Player->bHasAimPoint = true;
    Player->SetCameraMode(EPGCameraMode::QuarterView);
    TestTrue(TEXT("Returning restores quarter-view zoom"), FMath::IsNearlyEqual(Player->TargetCameraDistance, 950.f));
    TestTrue(TEXT("Returning restores fixed yaw and manual pitch"), Player->CameraBoom->GetComponentRotation().Equals(QuarterRotation, .01f));
    TestFalse(TEXT("Switching discards stale ground aim"), Player->bHasAimPoint);
    Player->SetCameraMode(EPGCameraMode::Action3D);
    TestTrue(TEXT("Action zoom is retained across switches"), FMath::IsNearlyEqual(Player->TargetCameraDistance, 350.f));
    Player->SetCameraMode(static_cast<EPGCameraMode>(255));
    TestTrue(TEXT("Invalid camera modes fall back to quarter-view"), Player->GetCameraMode() == EPGCameraMode::QuarterView);
    Player->CameraPitchOffset = 0.f;
    Player->TargetCameraDistance = Player->CameraMinOffset;
    Player->CameraBoom->TargetArmLength = Player->TargetCameraDistance;
    Player->UpdatePlayerCamera(0.f);
    TestTrue(TEXT("Close-up face pitch survives action mode round-trip"), FMath::IsNearlyEqual(Player->CameraBoom->GetComponentRotation().Pitch, Data->CloseUpPitch));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCameraCollisionTest, "PG.Camera.GroundAndWallCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCameraCollisionTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    // Exercise runtime PostInitializeComponents, including the controller camera manager.
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<APGCharacterPlayer>(FVector(0.f, 0.f, 96.f), FRotator::ZeroRotator);
    auto* Data = NewObject<UPGQuarterViewData>(Player);
    Player->QuarterViewData = Data;
    Player->CameraBoom->bDoCollisionTest = false; // Simulate stale Blueprint defaults.
    Player->FollowCamera->SetRelativeLocation(FVector(0.f, 0.f, -100.f));
    Player->ConfigureQuarterView();
    TestTrue(TEXT("Runtime restores collision after Blueprint defaults"), Player->CameraBoom->bDoCollisionTest);
    TestTrue(TEXT("View is attached at the swept socket"), Player->FollowCamera->GetRelativeLocation().IsNearlyZero());
    Player->SetCameraMode(EPGCameraMode::Action3D);
    auto AddBlocker = [&](FVector Location, FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        auto* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionProfileName(TEXT("BlockAll"));
        Box->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Box;
    };
    auto* Floor = AddBlocker(FVector(0.f, 0.f, -50.f), FVector(2000.f, 2000.f, 50.f));
    auto* Wall = AddBlocker(FVector(-200.f, 0.f, 250.f), FVector(10.f, 500.f, 250.f));
    auto Step = [&](float Pitch, float Yaw, float Dt)
    {
        Player->ActionCameraRotation = FRotator(Pitch, Yaw, 0.f);
        Player->UpdatePlayerCamera(Dt);
        Player->CameraBoom->TickComponent(Dt, LEVELTICK_All, nullptr);
        return Player->FollowCamera->GetComponentLocation();
    };
    const FVector GroundView = Step(Data->ActionMaxPitch, 0.f, 1.f / 60.f);
    TestTrue(TEXT("Upward view retracts before entering ground"), GroundView.Z >= Data->CameraProbeRadius - .1f);
    TestTrue(TEXT("Ground correction is active"), Player->CameraBoom->IsCollisionFixApplied());
    const FVector WallView = Step(0.f, 0.f, 1.f / 60.f);
    TestTrue(TEXT("Wall reserves sphere clearance"), WallView.X >= -190.f + Data->CameraProbeRadius - .1f);
    TestTrue(TEXT("Collision never overwrites requested zoom"), FMath::IsNearlyEqual(Player->TargetCameraDistance, Data->ActionDistance));
    Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const FVector ClearView = Step(0.f, 0.f, 1.f / 60.f);
    TestTrue(TEXT("Camera recovers full distance after obstruction clears"), ClearView.X < -400.f);
    // Reproduce a Blueprint capsule that still blocks Camera before runtime initialization.
    const FTransform EnemyTransform(FRotator::ZeroRotator, FVector(-200.f, 0.f, 96.f));
    auto* Enemy = World->SpawnActorDeferred<APGCharacterEnemy>(APGCharacterEnemy::StaticClass(), EnemyTransform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
    Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
    const auto VisibilityResponse = Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility);
    Enemy->FinishSpawning(EnemyTransform);
    TestEqual(TEXT("Existing enemy overrides cannot block Camera"), Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
    TestEqual(TEXT("Enemy targeting response is preserved"), Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility), VisibilityResponse);
    auto* Weapon = World->SpawnActor<APGWeaponBase>();
    Weapon->AttachToActor(Enemy, FAttachmentTransformRules::KeepRelativeTransform);
    TInlineComponentArray<UPrimitiveComponent*> WeaponPrimitives(Weapon);
    for (UPrimitiveComponent* Primitive : WeaponPrimitives)
        TestEqual(TEXT("Equipment ignores camera"), Primitive->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
    const FVector EnemyView = Step(0.f, 0.f, 1.f / 60.f);
    TestFalse(TEXT("Enemy on boom does not trigger collision retraction"), Player->CameraBoom->IsCollisionFixApplied());
    TestTrue(TEXT("Enemy on boom preserves full boom reach with camera lag"), EnemyView.X < -400.f);
    Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TestTrue(TEXT("Wall still blocks with enemy present"), Step(0.f, 0.f, 1.f / 60.f).X >= -190.f + Data->CameraProbeRadius - .1f);
    Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    const FVector BodyCenter = Player->GetCapsuleComponent()->GetComponentLocation();
    const float HideRadius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + Data->CameraBodyClearance;
    const FVector FadeMidpoint = BodyCenter + FVector(HideRadius + Data->CameraBodyFadeDistance * .5f, 0.f, 0.f);
    TestEqual(TEXT("Camera inside body is fully faded"), Player->GetCameraBodyFade(BodyCenter), 1.f);
    TestTrue(TEXT("Fade midpoint is continuous half coverage"), FMath::IsNearlyEqual(Player->GetCameraBodyFade(FadeMidpoint), .5f));
    TestEqual(TEXT("Distant camera is fully visible"), Player->GetCameraBodyFade(ClearView), 0.f);
    TestTrue(TEXT("Camera inside body hides local appearance"), Player->ShouldHideForCamera(BodyCenter, false));
    TestTrue(TEXT("Near plane clearance hides before penetration"), Player->ShouldHideForCamera(BodyCenter + FVector(HideRadius - 1.f, 0.f, 0.f), false));
    const FVector Boundary = BodyCenter + FVector(HideRadius + 1.f, 0.f, 0.f);
    TestFalse(TEXT("Outside entry threshold stays visible"), Player->ShouldHideForCamera(Boundary, false));
    TestTrue(TEXT("Exit hysteresis prevents flicker"), Player->ShouldHideForCamera(Boundary, true));
    TestFalse(TEXT("Clear view restores appearance"), Player->ShouldHideForCamera(ClearView, true));
    auto* PC = World->SpawnActor<APGPlayerController>();
    PC->SetPawn(Player);
    PC->SetViewTarget(Player);
    Weapon->AttachToActor(Player, FAttachmentTransformRules::KeepRelativeTransform);
    TSet<FPrimitiveComponentId> Hidden;
    Player->GetMesh()->SetRenderCustomDepth(true);
    PC->UpdateHiddenComponents(FadeMidpoint, Hidden);
    TestTrue(TEXT("Partial fade keeps mesh in view"), Hidden.IsEmpty());
    TestTrue(TEXT("Player receives continuous fade through CPD"), FMath::IsNearlyEqual(
        Player->GetMesh()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], .5f));
    TestTrue(TEXT("Equipment receives same fade"), FMath::IsNearlyEqual(
        Weapon->GetMeshComponent()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], .5f));
    TestFalse(TEXT("Partial fade does not outline dither holes"), Player->GetMesh()->bRenderCustomDepth);
    Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    PC->UpdateHiddenComponents(FadeMidpoint, Hidden);
    TestEqual(TEXT("Detached equipment restores fade"),
        Weapon->GetMeshComponent()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], 0.f);
    Weapon->AttachToActor(Player, FAttachmentTransformRules::KeepRelativeTransform);
    PC->UpdateHiddenComponents(BodyCenter, Hidden);
    TestTrue(TEXT("Local view excludes player mesh"), Hidden.Contains(Player->GetMesh()->GetPrimitiveSceneId()));
    TestTrue(TEXT("Local view excludes equipped weapon"), Hidden.Contains(Weapon->GetMeshComponent()->GetPrimitiveSceneId()));
    TestFalse(TEXT("Per-view hiding does not mutate shared mesh visibility"), Player->GetMesh()->bHiddenInGame);
    Hidden.Reset();
    PC->UpdateHiddenComponents(ClearView, Hidden);
    TestTrue(TEXT("Moving camera clear restores all view exclusions"), Hidden.IsEmpty());
    TestTrue(TEXT("Moving clear restores authored custom depth"), Player->GetMesh()->bRenderCustomDepth);
    TestEqual(TEXT("Moving clear resets fade data"), Player->GetMesh()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], 0.f);
    PC->SetViewTarget(Enemy);
    PC->UpdateHiddenComponents(BodyCenter, Hidden);
    TestTrue(TEXT("Other view targets never hide the player"), Hidden.IsEmpty());
    Player->SetCameraMode(EPGCameraMode::QuarterView);
    TestEqual(TEXT("Quarter-view never fades the body"), Player->GetCameraBodyFade(BodyCenter), 0.f);
    TestFalse(TEXT("Mode switch restores appearance"), Player->ShouldHideForCamera(BodyCenter, true));
    Player->SetCameraMode(EPGCameraMode::Action3D);
    // Slopes, large orbit changes, variable frame rates, and lag after movement.
    PC->SetViewTarget(Player);
    Enemy->SetActorLocation(FVector(5000.f, 0.f, 96.f));
    Enemy->GetMesh()->SetRenderCustomDepth(true);
    Weapon->AttachToActor(Enemy, FAttachmentTransformRules::KeepRelativeTransform);
    const FVector EnemyCenter = Enemy->GetCapsuleComponent()->GetComponentLocation();
    const float EnemyInner = Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() + Data->CameraBodyClearance;
    const FVector EnemyMidpoint = EnemyCenter + FVector(EnemyInner + Data->CameraBodyFadeDistance * .5f, 0.f, 0.f);
    Hidden.Reset();
    PC->UpdateHiddenComponents(EnemyMidpoint, Hidden);
    TestTrue(TEXT("Nearby enemy fades even with distant player"), FMath::IsNearlyEqual(
        Enemy->GetMesh()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], .5f));
    TestTrue(TEXT("Enemy equipment fades with its owner"), FMath::IsNearlyEqual(
        Weapon->GetMeshComponent()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], .5f));
    TestEqual(TEXT("Distant player stays visible while enemy fades"),
        Player->GetMesh()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], 0.f);
    TestTrue(TEXT("Partial enemy fade does not fully exclude body"), Hidden.IsEmpty());
    Enemy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PC->UpdateHiddenComponents(EnemyCenter, Hidden);
    TestTrue(TEXT("Camera inside enemy excludes body even after death disables collision"), Hidden.Contains(Enemy->GetMesh()->GetPrimitiveSceneId()));
    TestTrue(TEXT("Camera inside enemy excludes equipment"), Hidden.Contains(Weapon->GetMeshComponent()->GetPrimitiveSceneId()));
    Player->SetCameraMode(EPGCameraMode::QuarterView);
    Hidden.Reset();
    PC->UpdateHiddenComponents(EnemyCenter, Hidden);
    TestTrue(TEXT("Quarter-view never excludes nearby enemies"), Hidden.IsEmpty());
    TestEqual(TEXT("Quarter-view resets enemy fade"), Enemy->GetMesh()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], 0.f);
    TestTrue(TEXT("Quarter-view restores enemy outline"), Enemy->GetMesh()->bRenderCustomDepth);
    Player->SetCameraMode(EPGCameraMode::Action3D);
    PC->UpdateHiddenComponents(EnemyMidpoint, Hidden);
    PC->SetViewTarget(Enemy);
    PC->UpdateHiddenComponents(EnemyCenter, Hidden);
    TestEqual(TEXT("Other view target restores enemy fade"), Enemy->GetMesh()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], 0.f);
    PC->SetViewTarget(Player);
    PC->UpdateHiddenComponents(EnemyMidpoint, Hidden);
    Enemy->SetActorLocation(FVector(10000.f, 0.f, 96.f));
    PC->UpdateHiddenComponents(EnemyMidpoint, Hidden);
    TestEqual(TEXT("Enemy leaving camera restores fade"), Enemy->GetMesh()->GetCustomPrimitiveData().Data[APGPlayerController::CameraFadeDataIndex], 0.f);
    Floor->GetOwner()->SetActorRotation(FRotator(12.f, 0.f, 0.f));
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PGCameraCollisionTest), false, Player);
    for (float Dt : {1.f / 120.f, 1.f / 30.f, .1f})
    {
        for (float Yaw : {0.f, 90.f, 180.f, -90.f})
        {
            Player->SetActorLocation(FVector(20.f, 0.f, 110.f));
            const FVector View = Step(65.f, Yaw, Dt);
            TestFalse(TEXT("Camera sphere remains outside slope during orbit and lag"),
                World->OverlapBlockingTestByChannel(View, FQuat::Identity, ECC_Camera,
                    FCollisionShape::MakeSphere(Data->CameraProbeRadius - 1.f), Params));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCameraSettingsTest, "PG.Camera.SettingsPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCameraSettingsTest::RunTest(const FString&)
{
    const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / (TEXT("CameraSettings_") + FGuid::NewGuid().ToString() + TEXT(".ini")));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    ON_SCOPE_EXIT { IFileManager::Get().Delete(*Filename); };
    auto* Settings = NewObject<UPGCameraSettings>();
    Settings->CameraMode = EPGCameraMode::Action3D;
    Settings->SaveConfig(CPF_Config, *Filename);
    Settings->CameraMode = EPGCameraMode::QuarterView;
    Settings->LoadConfig(nullptr, *Filename);
    TestTrue(TEXT("Saved action camera survives reload"), Settings->GetCameraMode() == EPGCameraMode::Action3D);
    Settings->CameraMode = EPGCameraMode::QuarterView;
    Settings->SaveConfig(CPF_Config, *Filename);
    Settings->CameraMode = EPGCameraMode::Action3D;
    Settings->LoadConfig(nullptr, *Filename);
    TestTrue(TEXT("Quarter-view choice also survives reload"), Settings->GetCameraMode() == EPGCameraMode::QuarterView);
    Settings->CameraMode = static_cast<EPGCameraMode>(255);
    TestTrue(TEXT("Invalid saved modes use the default"), Settings->GetCameraMode() == EPGCameraMode::QuarterView);
    return true;
}
#endif

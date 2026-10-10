#include "PGShadingComparisonPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PGToonPreviewActor.h"
#include "PGActor/Components/Rendering/PGToonPresentationComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "PGUI/Widget/HUD/PGUIShadingComparison.h"

namespace
{
    const TCHAR* ComparisonIds[] = { TEXT("Bokusei"), TEXT("Arin"), TEXT("Hwarin"), TEXT("LianLian") };
    const TCHAR* ComparisonNames[] = { TEXT("Bokusei"), TEXT("아린"), TEXT("화련"), TEXT("Lianlian") };
}

APGShadingComparisonPawn::APGShadingComparisonPawn(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bAddDefaultMovementBindings = false;
    GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("ComparisonCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = 45.f;
}

void APGShadingComparisonPawn::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("PGShadingOverview")))
        {
            OverviewCamera = *It;
            break;
        }
    // The pawn can begin before the level's preview actors are initialized.
    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::InitializeComparisonLight);
}

void APGShadingComparisonPawn::InitializeComparisonLight()
{
    if (ComparisonKeyLight.IsValid())
    {
        UpdateLightHelp();
        return;
    }
    // Resolve the same authored light used by the materials. Never pick the fill light.
    for (TActorIterator<APGToonPreviewActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("PGShadingComparisonModel")) && IsValid(It->ToonPresentation->KeyLight))
        {
            ComparisonKeyLight = It->ToonPresentation->KeyLight;
            InitialLightRotation = ComparisonKeyLight->GetActorRotation();
            break;
        }
    UpdateLightHelp();
}

void APGShadingComparisonPawn::PawnClientRestart()
{
    Super::PawnClientRestart();
    if (APlayerController* PC = Cast<APlayerController>(GetController()); PC && PC->IsLocalController())
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetViewTarget(this);
        if (!HelpWidget) HelpWidget = CreateWidget<UPGUIShadingComparison>(PC);
        if (HelpWidget)
        {
            HelpWidget->SetShadowEnabled(bShadowCasterEnabled);
            HelpWidget->SetHairShadowEnabled(bHairShadowEnabled);
            UpdateLightHelp();
            HelpWidget->AddToViewport();
        }
        ShowOverview();
    }
}

void APGShadingComparisonPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HelpWidget) HelpWidget->RemoveFromParent();
    HelpWidget = nullptr;
    Super::EndPlay(Reason);
}

void APGShadingComparisonPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ThisClass::LookHorizontal);
    PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ThisClass::LookVertical);
    const FKey StageKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(StageKeys); ++Index)
    {
        FInputKeyBinding Binding(FInputChord(StageKeys[Index]), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ThisClass::FocusStage, Index);
        PlayerInputComponent->KeyBindings.Add(MoveTemp(Binding));
    }
    PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ThisClass::ShowOverview);
    PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::ShowOverview);
    PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::NextModel);
    PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::ShowFace);
    PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ThisClass::ShowQuarter);
    PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &ThisClass::ToggleShadowCaster);
    PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ThisClass::ToggleHairShadow);
    PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &ThisClass::ToggleLightOrbit);
    PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &ThisClass::ResetLight);
    const FKey LightKeys[] = { EKeys::Z, EKeys::X, EKeys::V };
    const float Azimuths[] = { 0.f, 60.f, 180.f };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(LightKeys); ++Index)
    {
        FInputKeyBinding Binding(FInputChord(LightKeys[Index]), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ThisClass::SetLightAngles, Azimuths[Index], 35.f);
        PlayerInputComponent->KeyBindings.Add(MoveTemp(Binding));
    }
}

void APGShadingComparisonPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !PC->IsLocalController()) return;
    auto Axis = [PC](const FKey& Positive, const FKey& Negative)
    {
        return float(PC->IsInputKeyDown(Positive)) - float(PC->IsInputKeyDown(Negative));
    };
    const float Speed = FMath::Max(10.f, MoveSpeed) * (PC->IsInputKeyDown(EKeys::LeftShift) ? FMath::Max(1.f, FastMultiplier) : 1.f);
    if (UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(GetMovementComponent()))
    {
        Movement->MaxSpeed = Speed;
        Movement->Acceleration = Speed * 8.f;
        Movement->Deceleration = Speed * 10.f;
    }
    const FRotationMatrix View(PC->GetControlRotation());
    const FVector Direction = View.GetUnitAxis(EAxis::X) * Axis(EKeys::W, EKeys::S)
        + View.GetUnitAxis(EAxis::Y) * Axis(EKeys::D, EKeys::A)
        + FVector::UpVector * Axis(EKeys::E, EKeys::Q);
    AddMovementInput(Direction.GetClampedToMaxSize(1.f));
    if (ComparisonKeyLight.IsValid())
    {
        const float Horizontal = Axis(EKeys::Right, EKeys::Left);
        const float Vertical = Axis(EKeys::Up, EKeys::Down);
        // Manual control stops the sweep at its current direction.
        if (Horizontal != 0.f || Vertical != 0.f) bLightOrbitEnabled = false;
        if (Horizontal != 0.f || Vertical != 0.f || bLightOrbitEnabled)
        {
            const FRotator Rotation = ComparisonKeyLight->GetActorRotation();
            const float SpeedDegrees = FMath::Max(1.f, LightRotationSpeed);
            ApplyLightAngles(-90.f - Rotation.Yaw + DeltaSeconds * (Horizontal * SpeedDegrees
                + (bLightOrbitEnabled ? FMath::Max(1.f, LightOrbitSpeed) : 0.f)),
                -Rotation.Pitch + DeltaSeconds * Vertical * SpeedDegrees);
        }
    }
}

void APGShadingComparisonPawn::LookHorizontal(float Value)
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()); PC && PC->IsInputKeyDown(EKeys::RightMouseButton))
    {
        FRotator Rotation = PC->GetControlRotation();
        Rotation.Yaw += Value * LookSensitivity;
        PC->SetControlRotation(Rotation);
    }
}

void APGShadingComparisonPawn::LookVertical(float Value)
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()); PC && PC->IsInputKeyDown(EKeys::RightMouseButton))
    {
        FRotator Rotation = PC->GetControlRotation();
        Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) + Value * LookSensitivity, -89.f, 89.f);
        PC->SetControlRotation(Rotation);
    }
}

void APGShadingComparisonPawn::SetView(const FVector& Location, const FRotator& Rotation)
{
    GetMovementComponent()->StopMovementImmediately();
    SetActorLocation(Location);
    if (AController* PC = GetController()) PC->SetControlRotation(Rotation);
}

void APGShadingComparisonPawn::ShowOverview()
{
    bOverview = true;
    SelectedView = EComparisonView::Front;
    FVector Offset = FVector::ZeroVector;
    if (AActor* Stage = FindStage(SelectedModel, 0))
        if (AActor* Original = FindStage(0, 0)) Offset = Stage->GetActorLocation() - Original->GetActorLocation();
    if (SelectedModel != 0) Offset.Y += 250.f; // Include the full width of the end-stage signs.
    if (OverviewCamera.IsValid())
        SetView(OverviewCamera->GetActorLocation() + Offset, OverviewCamera->GetActorRotation());
    if (HelpWidget) HelpWidget->SetViewLabel(FText::FromString(FString::Printf(TEXT("%s · 전체 단계 비교"), ComparisonNames[SelectedModel])));
}

AActor* APGShadingComparisonPawn::FindStage(int32 Model, int32 Stage) const
{
    const FName Tag(* (Model == 0 ? FString::Printf(TEXT("PGShadingStage%d"), Stage)
        : FString::Printf(TEXT("PGShadingGuestStage_%s_%d"), ComparisonIds[Model], Stage)));
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(Tag)) return *It;
    return nullptr;
}

void APGShadingComparisonPawn::NextModel()
{
    for (int32 Step = 1; Step < UE_ARRAY_COUNT(ComparisonIds); ++Step)
    {
        const int32 Candidate = (SelectedModel + Step) % UE_ARRAY_COUNT(ComparisonIds);
        if (!FindStage(Candidate, SelectedStage)) continue;
        SelectedModel = Candidate;
        if (bOverview) ShowOverview();
        else FocusStage(SelectedStage);
        return;
    }
}

void APGShadingComparisonPawn::FocusStage(int32 Index)
{
    if (Index < 0 || Index >= 8) return;
    if (AActor* Stage = FindStage(SelectedModel, Index))
        {
            bOverview = false;
            SelectedStage = Index;
            const FVector Base = Stage->GetActorLocation();
            const bool bFace = SelectedView == EComparisonView::Face;
            const bool bQuarter = SelectedView == EComparisonView::Quarter;
            FVector Target = Base + FVector(0, 0, bFace ? 139 : 90);
            FVector Location = Base + (bFace ? FVector(0, 135, 144) : bQuarter ? FVector(230, 390, 330) : FVector(0, 360, 125));
            if (bFace && SelectedModel != 0)
                if (const auto* Preview = Cast<APGToonPreviewActor>(Stage))
                {
                    Target = Preview->GetSkeletalMeshComponent()->GetSocketLocation(Preview->ToonPresentation->HeadBone);
                    Location = Target + FVector(0, 135, 5);
                }
            SetView(Location, (Target - Location).Rotation());
            static const TCHAR* Names[] = { TEXT("일반 조명"), TEXT("셀 명암"), TEXT("부위별 명암"), TEXT("림·하이라이트"), TEXT("외곽선 · 기존 툰"), TEXT("월드 그림자"), TEXT("얼굴 SDF"), TEXT("얼굴 SDF · 월드 그림자") };
            if (HelpWidget) HelpWidget->SetViewLabel(FText::FromString(FString::Printf(TEXT("%s · %d단계 · %s"), ComparisonNames[SelectedModel], Index + 1, Names[Index])));
            return;
        }
}

void APGShadingComparisonPawn::ShowFace()
{
    SelectedView = EComparisonView::Face;
    FocusStage(SelectedStage);
}

void APGShadingComparisonPawn::ShowQuarter()
{
    SelectedView = EComparisonView::Quarter;
    FocusStage(SelectedStage);
}

void APGShadingComparisonPawn::ToggleShadowCaster()
{
    bShadowCasterEnabled = !bShadowCasterEnabled;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("PGShadingShadowCaster")) || It->ActorHasTag(TEXT("PGShadingGuestShadowCaster")))
        {
            TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
            for (UPrimitiveComponent* Primitive : Primitives)
                Primitive->SetCastShadow(bShadowCasterEnabled);
        }
    if (HelpWidget) HelpWidget->SetShadowEnabled(bShadowCasterEnabled);
}

void APGShadingComparisonPawn::ToggleHairShadow()
{
    bHairShadowEnabled = !bHairShadowEnabled;
    for (TActorIterator<APGToonPreviewActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("PGShadingStage5")) || It->ActorHasTag(TEXT("PGShadingStage7")) || It->ActorHasTag(TEXT("PGShadingGuestWorld")))
        {
            if (It->HairShadowProxy->GetSkeletalMeshAsset()) It->HairShadowProxy->SetCastShadow(bHairShadowEnabled);
            It->ToonPresentation->bHairShadowEnabled = bHairShadowEnabled;
            if (auto* Proxy = It->ToonPresentation->GetHairShadowProxy()) Proxy->SetCastShadow(bHairShadowEnabled);
        }
    if (HelpWidget) HelpWidget->SetHairShadowEnabled(bHairShadowEnabled);
}

void APGShadingComparisonPawn::ApplyLightAngles(float Azimuth, float Elevation)
{
    if (!ComparisonKeyLight.IsValid() || !FMath::IsFinite(Azimuth) || !FMath::IsFinite(Elevation)) return;
    ComparisonKeyLight->SetActorRotation(FRotator(-FMath::Clamp(Elevation, -85.f, 85.f),
        FRotator::NormalizeAxis(-90.f - Azimuth), 0.f));
    UpdateLightHelp();
}

void APGShadingComparisonPawn::SetLightAngles(float Azimuth, float Elevation)
{
    if (!FMath::IsFinite(Azimuth) || !FMath::IsFinite(Elevation)) return;
    bLightOrbitEnabled = false;
    ApplyLightAngles(Azimuth, Elevation);
}

void APGShadingComparisonPawn::ToggleLightOrbit()
{
    bLightOrbitEnabled = ComparisonKeyLight.IsValid() && !bLightOrbitEnabled;
    UpdateLightHelp();
}

void APGShadingComparisonPawn::ResetLight()
{
    bLightOrbitEnabled = false;
    if (ComparisonKeyLight.IsValid()) ComparisonKeyLight->SetActorRotation(InitialLightRotation);
    UpdateLightHelp();
}

void APGShadingComparisonPawn::UpdateLightHelp()
{
    if (!HelpWidget) return;
    const bool bAvailable = ComparisonKeyLight.IsValid();
    const FRotator Rotation = bAvailable ? ComparisonKeyLight->GetActorRotation() : FRotator::ZeroRotator;
    HelpWidget->SetLightState(bAvailable, FRotator::NormalizeAxis(-90.f - Rotation.Yaw), -Rotation.Pitch, bLightOrbitEnabled);
}

bool APGShadingComparisonPawn::SendProbeInput(FKey Key, bool bPressed, float AxisValue)
{
#if !UE_BUILD_SHIPPING
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC && PC->IsLocalController() && FParse::Param(FCommandLine::Get(), TEXT("PGShadingComparisonProbe")))
    {
        if (Key.IsAxis1D())
            return PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key,
                AxisValue, 1.f / 60.f, 1, FPlatformTime::Cycles64()));
        return PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key,
            bPressed ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
    }
#endif
    return false;
}

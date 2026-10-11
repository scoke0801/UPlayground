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
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
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
    const TCHAR* ComparisonNames[] = { TEXT(""), TEXT("아린"), TEXT("화련"), TEXT("Lianlian") };

    // The viewport console consumes Tilde before pawn bindings. Intercept it only
    // while this local comparison viewport has keyboard focus; never change console settings.
    class FPGComparisonHelpInputProcessor final : public IInputProcessor
    {
    public:
        explicit FPGComparisonHelpInputProcessor(APGShadingComparisonPawn* InPawn) : Pawn(InPawn) {}
        virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
        virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            if (Event.GetKey() != EKeys::Tilde || Event.IsControlDown() || Event.IsAltDown() || Event.IsCommandDown()) return false;
            APGShadingComparisonPawn* Owner = Pawn.Get();
            if (!Owner || !Owner->IsLocallyControlled() || !Owner->GetWorld()) return false;
            UGameViewportClient* Viewport = Owner->GetWorld()->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (!Widget || !(Widget->HasKeyboardFocus() || Widget->HasFocusedDescendants())) return false;
            if (!Event.IsRepeat()) Owner->ToggleHelp();
            return true;
        }
    private:
        TWeakObjectPtr<APGShadingComparisonPawn> Pawn;
    };
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
            HelpWidget->SetVisibility(bHelpVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
        if (!HelpInputProcessor && FSlateApplication::IsInitialized())
        {
            HelpInputProcessor = MakeShared<FPGComparisonHelpInputProcessor>(this);
            FSlateApplication::Get().RegisterInputPreProcessor(HelpInputProcessor, 0);
        }
        ShowOverview();
    }
}

void APGShadingComparisonPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    StopStageSequence();
    if (HelpInputProcessor && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(HelpInputProcessor);
    HelpInputProcessor.Reset();
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
    PlayerInputComponent->BindKey(EKeys::Tilde, IE_Pressed, this, &ThisClass::ToggleHelp);
    PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &ThisClass::ToggleStageSequence);
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
    if (!Direction.IsNearlyZero() || PC->IsInputKeyDown(EKeys::RightMouseButton)) StopStageSequence();
    TickStageSequence(DeltaSeconds);
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
    StopStageSequence();
    bOverview = true;
    SelectedView = EComparisonView::Front;
    FVector Offset = FVector::ZeroVector;
    if (AActor* Stage = FindStage(SelectedModel, 0))
        if (AActor* Original = FindStage(0, 0)) Offset = Stage->GetActorLocation() - Original->GetActorLocation();
    if (SelectedModel != 0) Offset.Y += 250.f; // Include the full width of the end-stage signs.
    if (OverviewCamera.IsValid())
        SetView(OverviewCamera->GetActorLocation() + Offset, OverviewCamera->GetActorRotation());
    UpdateViewHelp();
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
    const AActor* Previous = FindStage(SelectedModel, bOverview ? 0 : SelectedStage);
    if (!Previous) return;
    for (int32 Step = 1; Step < UE_ARRAY_COUNT(ComparisonIds); ++Step)
    {
        const int32 Candidate = (SelectedModel + Step) % UE_ARRAY_COUNT(ComparisonIds);
        const AActor* Next = FindStage(Candidate, bOverview ? 0 : SelectedStage);
        if (!Next) continue;
        const FVector Offset = Next->GetActorLocation() - Previous->GetActorLocation();
        SelectedModel = Candidate;
        // Translate the user's framing, including an in-progress sequence, without
        // reapplying presets or resetting their viewing angle, height or distance.
        SetActorLocation(GetActorLocation() + Offset);
        StageSequenceStart += Offset;
        UpdateViewHelp();
        return;
    }
}

void APGShadingComparisonPawn::FocusStage(int32 Index)
{
    if (Index < 0 || Index >= 8) return;
    if (AActor* Stage = FindStage(SelectedModel, Index))
        {
            StopStageSequence();
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
            UpdateViewHelp();
            return;
        }
}

void APGShadingComparisonPawn::UpdateViewHelp()
{
    if (!HelpWidget) return;
    static const TCHAR* Names[] = { TEXT("일반 조명"), TEXT("셀 명암"), TEXT("부위별 명암"), TEXT("림·하이라이트"), TEXT("외곽선"), TEXT("월드 그림자"), TEXT("얼굴 SDF"), TEXT("얼굴 SDF · 월드 그림자") };
    const FString ModelPrefix = FString(ComparisonNames[SelectedModel]).IsEmpty()
        ? FString() : FString::Printf(TEXT("%s · "), ComparisonNames[SelectedModel]);
    HelpWidget->SetViewLabel(FText::FromString(bOverview
        ? ModelPrefix + TEXT("전체 단계 비교")
        : FString::Printf(TEXT("%s%d단계 · %s%s"), *ModelPrefix, SelectedStage + 1,
            Names[SelectedStage], bStageSequencePlaying ? TEXT(" · 자동 진행 중") : TEXT(""))));
}

void APGShadingComparisonPawn::ToggleHelp()
{
    bHelpVisible = !bHelpVisible;
    if (HelpWidget) HelpWidget->SetVisibility(bHelpVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void APGShadingComparisonPawn::StopStageSequence()
{
    if (!bStageSequencePlaying) return;
    bStageSequencePlaying = false;
    UpdateViewHelp();
}

void APGShadingComparisonPawn::ToggleStageSequence()
{
    if (bStageSequencePlaying)
    {
        StopStageSequence();
        return;
    }
    const AActor* FirstStage = FindStage(SelectedModel, 0);
    if (!FirstStage) return;
    FocusStage(0);
    // Frame the pedestal and the highest stage sign (225 cm), even when P starts in face view.
    int32 Width = 0, Height = 0;
    const auto* PC = Cast<APlayerController>(GetController());
    if (PC) PC->GetViewportSize(Width, Height);
    const float Aspect = FMath::Max(.1f, Camera->bConstrainAspectRatio ? Camera->AspectRatio
        : Height > 0 ? static_cast<float>(Width) / Height : Camera->AspectRatio);
    const auto* Player = PC ? PC->GetLocalPlayer() : nullptr;
    const EAspectRatioAxisConstraint Axis = Camera->bOverrideAspectRatioAxisConstraint ? Camera->AspectRatioAxisConstraint.GetValue()
        : Player ? Player->AspectRatioAxisConstraint.GetValue() : AspectRatio_MaintainXFOV;
    const bool bHorizontalFOV = Camera->bConstrainAspectRatio || Axis == AspectRatio_MaintainXFOV ||
        (Axis == AspectRatio_MajorAxisFOV && Aspect > 1.f);
    const float HalfFOV = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Camera->FieldOfView, 10.f, 120.f) * .5f));
    const float VerticalFOV = HalfFOV / (bHorizontalFOV ? Aspect : FMath::Max(.1f, Camera->AspectRatio));
    const float HorizontalFOV = bHorizontalFOV ? HalfFOV : VerticalFOV * Aspect;
    const float Distance = FMath::Max(360.f, FMath::Max(120.f / HorizontalFOV, 145.f / VerticalFOV));
    SetView(FirstStage->GetActorLocation() + FVector(0, Distance, 110), FRotator(0, -90, 0));
    StageSequenceStart = GetActorLocation();
    StageSequenceElapsed = 0.f;
    bStageSequencePlaying = true;
    UpdateViewHelp();
}

void APGShadingComparisonPawn::TickStageSequence(float DeltaSeconds)
{
    if (!bStageSequencePlaying) return;
    const AActor* Current = FindStage(SelectedModel, SelectedStage);
    const AActor* Next = SelectedStage < 7 ? FindStage(SelectedModel, SelectedStage + 1) : nullptr;
    if (!Current || !Next)
    {
        StopStageSequence();
        return;
    }
    const float Hold = FMath::Max(0.f, StageHoldSeconds);
    const float Duration = FMath::Max(.1f, StageMoveSeconds);
    StageSequenceElapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp((StageSequenceElapsed - Hold) / Duration, 0.f, 1.f);
    const float SmoothAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
    const FVector Destination = StageSequenceStart + Next->GetActorLocation() - Current->GetActorLocation();
    SetActorLocation(FMath::Lerp(StageSequenceStart, Destination, SmoothAlpha));
    if (Alpha >= 1.f)
    {
        ++SelectedStage;
        StageSequenceStart = Destination;
        StageSequenceElapsed = 0.f;
        if (SelectedStage == 7) bStageSequencePlaying = false;
        UpdateViewHelp();
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
        // Exercise the real pre-console Slate route for the conflicting console key.
        if (Key == EKeys::Tilde && FSlateApplication::IsInitialized())
        {
            UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (!Widget) return false;
            FSlateApplication& Slate = FSlateApplication::Get();
            Slate.SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
            const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
            return bPressed ? Slate.ProcessKeyDownEvent(Event) : Slate.ProcessKeyUpEvent(Event);
        }
        if (Key.IsAxis1D())
            return PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key,
                AxisValue, 1.f / 60.f, 1, FPlatformTime::Cycles64()));
        return PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key,
            bPressed ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
    }
#endif
    return false;
}

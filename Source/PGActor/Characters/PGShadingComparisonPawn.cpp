#include "PGShadingComparisonPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGUI/Widget/HUD/PGUIShadingComparison.h"

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
        if (HelpWidget) HelpWidget->AddToViewport();
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
    const FKey StageKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(StageKeys); ++Index)
    {
        FInputKeyBinding Binding(FInputChord(StageKeys[Index]), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ThisClass::FocusStage, Index);
        PlayerInputComponent->KeyBindings.Add(MoveTemp(Binding));
    }
    PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ThisClass::ShowOverview);
    PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::ShowOverview);
    PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::ShowFace);
    PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ThisClass::ShowQuarter);
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
    SelectedView = EComparisonView::Front;
    if (OverviewCamera.IsValid())
        SetView(OverviewCamera->GetActorLocation(), OverviewCamera->GetActorRotation());
    if (HelpWidget) HelpWidget->SetViewLabel(FText::FromString(TEXT("전체 단계 비교")));
}

void APGShadingComparisonPawn::FocusStage(int32 Index)
{
    if (Index < 0 || Index >= 5) return;
    const FName Tag(*FString::Printf(TEXT("PGShadingStage%d"), Index));
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(Tag))
        {
            SelectedStage = Index;
            const FVector Base = It->GetActorLocation();
            const bool bFace = SelectedView == EComparisonView::Face;
            const bool bQuarter = SelectedView == EComparisonView::Quarter;
            const FVector Target = Base + FVector(0, 0, bFace ? 139 : 90);
            const FVector Location = Base + (bFace ? FVector(0, 135, 144) : bQuarter ? FVector(230, 390, 330) : FVector(0, 360, 125));
            SetView(Location, (Target - Location).Rotation());
            static const TCHAR* Names[] = { TEXT("일반 조명"), TEXT("셀 명암"), TEXT("부위별 명암"), TEXT("림·하이라이트"), TEXT("외곽선 · 완성") };
            if (HelpWidget) HelpWidget->SetViewLabel(FText::FromString(FString::Printf(TEXT("%d단계 · %s"), Index + 1, Names[Index])));
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

// Fill out your copyright notice in the Description page of Project Settings.

#include "PGPlayerController.h"
#include "Components/MeshComponent.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGUI/Widget/Window/PGUIInventory.h"
#include "PGUI/Widget/Window/PGUISettings.h"
#include "PGUI/Widget/HUD/PGUIMainHUD.h"
#include "PGUI/Widget/Billboard/PGUILootOverlay.h"
#include "PGUI/Manager/PGUIManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "PGActor/Interface/PGClickableInterface.h"

APGPlayerController::APGPlayerController(const FObjectInitializer& ObjectInitializer)
{
	PlayerTeamId = FGenericTeamId(0);
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void APGPlayerController::ResetCameraFade()
{
    for (const auto& Entry : CameraFadedMeshes)
        if (UMeshComponent* Mesh = Entry.Key.Get())
        {
            Mesh->SetCustomPrimitiveDataFloat(CameraFadeDataIndex, 0.f);
            Mesh->SetRenderCustomDepth(Entry.Value);
        }
    CameraFadedMeshes.Reset();
}

void APGPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
    Super::UpdateHiddenComponents(ViewLocation, HiddenComponents);
    APGCharacterPlayer* ViewCharacter = Cast<APGCharacterPlayer>(GetPawn());
    if (!ViewCharacter || GetViewTarget() != ViewCharacter || ViewCharacter->GetCameraMode() != EPGCameraMode::Action3D)
    {
        ResetCameraFade();
        CameraHiddenPlayer.Reset();
        return;
    }
    if (CameraHiddenPlayer.Get() != ViewCharacter) ResetCameraFade();
    CameraHiddenPlayer = ViewCharacter;
    TSet<TWeakObjectPtr<UMeshComponent>> CurrentMeshes;
    auto FadeCharacter = [&](ACharacter* Subject, float Fade)
    {
        if (Fade <= 0.f) return;
        TArray<AActor*> Actors;
        Subject->GetAttachedActors(Actors, true, true);
        Actors.Add(Subject);
        for (AActor* Actor : Actors)
        {
            TInlineComponentArray<UMeshComponent*> Meshes(Actor);
            for (UMeshComponent* Mesh : Meshes)
            {
                CurrentMeshes.Add(Mesh);
                if (!CameraFadedMeshes.Contains(Mesh)) CameraFadedMeshes.Add(Mesh, Mesh->bRenderCustomDepth);
                Mesh->SetCustomPrimitiveDataFloat(CameraFadeDataIndex, Fade);
                // A depth-derived outline would trace every dither hole. Suppress it while fading.
                Mesh->SetRenderCustomDepth(false);
                if (Fade >= 1.f) HiddenComponents.Add(Mesh->GetPrimitiveSceneId());
            }
        }
    };
    FadeCharacter(ViewCharacter, ViewCharacter->GetCameraBodyFade(ViewLocation));
    // Cheap capsule checks only; enumerate meshes/attachments for nearby enemies.
    // Do not rely on collision overlaps: death disables queries before the body disappears.
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
        FadeCharacter(*It, ViewCharacter->GetCameraBodyFade(ViewLocation, It->GetCapsuleComponent()));
    // Equipment can be detached/replaced while the camera is inside the fade zone.
    for (auto It = CameraFadedMeshes.CreateIterator(); It; ++It)
        if (!CurrentMeshes.Contains(It.Key()))
        {
            if (UMeshComponent* Mesh = It.Key().Get())
            {
                Mesh->SetCustomPrimitiveDataFloat(CameraFadeDataIndex, 0.f);
                Mesh->SetRenderCustomDepth(It.Value());
            }
            It.RemoveCurrent();
        }
}

ETeamAttitude::Type APGPlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
	return IGenericTeamAgentInterface::GetTeamAttitudeTowards(Other);
}

void APGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		const TSubclassOf<UUserWidget> SelectedHUD = bUseLegacyHUD ? HUDWidgetClass.Get() : UPGUIMainHUD::StaticClass();
		if (UUserWidget* Widget = SelectedHUD ? CreateWidget(this, SelectedHUD) : nullptr)
		{
			Widget->AddToViewport();
		}	
        LootOverlay = CreateWidget<UPGUILootOverlay>(this);
        if (LootOverlay) LootOverlay->AddToViewport(1);
	}

	// Enhanced Input 서브시스템 설정
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = 
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (ClickMappingContext)
		{
			Subsystem->AddMappingContext(ClickMappingContext, 1);
		}
	}

	// 마우스 커서 표시 및 입력 모드 설정
	bShowMouseCursor = true;
	
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false); // 커서 숨김 방지
	SetInputMode(InputMode);
    RefreshCameraInputMode();
}

void APGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ToggleSettings).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ThisClass::ToggleInventory).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::PickupNearest);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ClickAction)
		{
			// Enhanced Input 방식으로 바인딩
			EnhancedInputComponent->BindAction(ClickAction, ETriggerEvent::Started, 
				this, &APGPlayerController::HandleMouseClick);
		}
	}
}

void APGPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    RefreshCameraInputMode();

	// 마우스 오버 체크 활성화 시
	if (bEnableMouseOverCheck && bShowMouseCursor)
	{
		// 마우스 이동량 확인
		FVector2D MouseDelta;
		GetInputMouseDelta(MouseDelta.X, MouseDelta.Y);
		
		// 마우스가 실제로 움직였을 때만 체크
		if (MouseDelta.SizeSquared() > MouseMovementThreshold)
		{
			CheckMouseOver();
		}
	}
}

void APGPlayerController::HandleMouseClick()
{
	bLastClickConsumed = false;
    if (!bShowMouseCursor) return;
    if (IsMoveInputIgnored() || IsPointerOverUI()) { bLastClickConsumed = true; return; }
	
	FVector HitLocation;
	AActor* ClickedActor = GetActorUnderCursor(HitLocation);

	// 이전 클릭 대상과 다른 경우, 이전 대상의 클릭 취소
	if (LastClickedActor.IsValid() && LastClickedActor.Get() != ClickedActor)
	{
		if (LastClickedActor->Implements<UPGClickableInterface>())
		{
			IPGClickableInterface* ClickableInterface = Cast<IPGClickableInterface>(LastClickedActor.Get());
			if (ClickableInterface)
			{
				ClickableInterface->Execute_OnClickCancelled(LastClickedActor.Get());
			}
		}
	}

	// 새로운 액터 클릭 처리
	if (ClickedActor && ClickedActor->Implements<UPGClickableInterface>())
	{
		IPGClickableInterface* ClickableInterface = Cast<IPGClickableInterface>(ClickedActor);
		if (ClickableInterface && ClickableInterface->Execute_IsClickable(ClickedActor))
		{
			ClickableInterface->Execute_OnClicked(ClickedActor, ClickedActor, HitLocation);
			LastClickedActor = ClickedActor;
			
			// 클릭 가능한 액터를 실제로 클릭했으므로 입력 소비
			bLastClickConsumed = true;
		}
	}
	else
	{
		// 빈 공간 클릭 시, 이전 클릭 취소
		if (LastClickedActor.IsValid())
		{
			if (LastClickedActor->Implements<UPGClickableInterface>())
			{
				IPGClickableInterface* ClickableInterface = Cast<IPGClickableInterface>(LastClickedActor.Get());
				if (ClickableInterface)
				{
					ClickableInterface->Execute_OnClickCancelled(LastClickedActor.Get());
				}
			}
			LastClickedActor = nullptr;
		}
	}
}

AActor* APGPlayerController::GetActorUnderCursor(FVector& OutHitLocation) const
{
	FVector WorldLocation, WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return nullptr;
	}

	FVector TraceStart = WorldLocation;
	FVector TraceEnd = WorldLocation + (WorldDirection * ClickTraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetPawn());

	if (GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ClickTraceChannel,
		QueryParams))
	{
		OutHitLocation = HitResult.ImpactPoint;
		return HitResult.GetActor();
	}

	return nullptr;
}

void APGPlayerController::CheckMouseOver()
{
	FVector HitLocation;
	AActor* CurrentHoveredActor = GetActorUnderCursor(HitLocation);

	// 이전 액터와 다른 경우
	if (LastHoveredActor.IsValid() && LastHoveredActor.Get() != CurrentHoveredActor)
	{
		// 이전 액터의 OnMouseLeave 호출
		if (LastHoveredActor->Implements<UPGClickableInterface>())
		{
			IPGClickableInterface* ClickableInterface = Cast<IPGClickableInterface>(LastHoveredActor.Get());
			if (ClickableInterface)
			{
				ClickableInterface->Execute_OnMouseLeave(LastHoveredActor.Get());
			}
		}
	}

	// 새로운 액터에 마우스 오버
	if (CurrentHoveredActor && CurrentHoveredActor->Implements<UPGClickableInterface>())
	{
		IPGClickableInterface* ClickableInterface = Cast<IPGClickableInterface>(CurrentHoveredActor);
		if (ClickableInterface && ClickableInterface->Execute_IsClickable(CurrentHoveredActor))
		{
			// 새로운 액터인 경우에만 OnMouseOver 호출
			if (!LastHoveredActor.IsValid() || LastHoveredActor.Get() != CurrentHoveredActor)
			{
				ClickableInterface->Execute_OnMouseOver(CurrentHoveredActor);
			}
		}
	}

	LastHoveredActor = CurrentHoveredActor;
}

bool APGPlayerController::IsPointerOverUI() const
{
    if (!bShowMouseCursor || !FSlateApplication::IsInitialized()) return false;
    FSlateApplication& Slate = FSlateApplication::Get();
    const FWidgetPath Path = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows());
    for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
    {
        const FName Type = Path.Widgets[Index].Widget->GetType();
        if (Type == FName(TEXT("SObjectWidget")) || Type == FName(TEXT("SButton"))) return true;
    }
    return false;
}
void APGPlayerController::CloseInventory()
{
    if (!InventoryWidget) return;
    if (auto* UI = UPGUIManager::Get(this)) UI->ReleaseModalInput(InventoryWidget);
    InventoryWidget->RemoveFromParent(); InventoryWidget = nullptr;
    if (auto* LocalPawn = Cast<APGCharacterPlayer>(GetPawn()))
        if (auto* ASC = LocalPawn->GetPGAbilitySystemComponent()) ASC->ClearBufferedInput();
}

void APGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetCameraFade();
    if (LootOverlay) { LootOverlay->RemoveFromParent(); LootOverlay = nullptr; }
    CloseSettings();
    CloseInventory();
    Super::EndPlay(EndPlayReason);
}

void APGPlayerController::ToggleInventory()
{
    if (InventoryWidget)
    {
        CloseInventory(); return;
    }
    auto* LocalCharacter = Cast<APGCharacterPlayer>(GetPawn());
    if (!LocalCharacter || !LocalCharacter->IsGameplayInputAllowed()) return;
    auto* UI = UPGUIManager::Get(this);
    if (!UI) return;
    LocalCharacter->GetPGAbilitySystemComponent()->CancelAllAbilities();
    LocalCharacter->GetPGAbilitySystemComponent()->ClearBufferedInput();
    InventoryWidget = CreateWidget<UPGUIInventory>(this);
    if (!InventoryWidget) return;
    InventoryWidget->AddToViewport(90);
    bool bBuildPhase = false;
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It)
          if (It->GetCurrentStageState() == EPGStageState::BuildPhase || It->GetCurrentStageState() == EPGStageState::RunPreparation) { bBuildPhase = true; break; }
    UI->AcquireModalInput(InventoryWidget, !bBuildPhase);
}
void APGPlayerController::PickupNearest()
{
    auto* LocalCharacter = Cast<APGCharacterPlayer>(GetPawn());
    if (!LocalCharacter || !LocalCharacter->IsGameplayInputAllowed()) return;
    if (auto* Nearest = APGLootDrop::FindNearestPickup(LocalCharacter)) Nearest->TryPickup(LocalCharacter);
}

void APGPlayerController::PGHUDCapture()
{
#if !UE_BUILD_SHIPPING
    if (!IsLocalController() || !GetWorld()) return;
    FTimerHandle CaptureTimer;
    GetWorldTimerManager().SetTimer(CaptureTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UPGUIMainHUD::StaticClass(), true);
        UE_LOG(LogTemp, Display, TEXT("PGMainUI capture: native HUD count=%d"), Widgets.Num());
        FScreenshotRequest::RequestScreenshot(TEXT("PGMainHUD"), true, true);
    }), 3.f, false);
#endif
}

void APGPlayerController::ToggleSettings()
{
    if (SettingsWidget) { CloseSettings(); return; }
    if (!IsLocalController()) return;
    // Do not discard unsaved inventory edits or cover a reward/result decision.
    auto* UI = UPGUIManager::Get(this);
    if (!UI || UI->IsWindowOpen()) return;
    SettingsWidget = CreateWidget<UPGUISettings>(this);
    if (!SettingsWidget) return;
    if (auto* LocalCharacter = Cast<APGCharacterPlayer>(GetPawn()))
        if (auto* ASC = LocalCharacter->GetPGAbilitySystemComponent()) ASC->ClearBufferedInput();
    SettingsWidget->AddToViewport(110);
    UI->AcquireModalInput(SettingsWidget, true);
    SettingsWidget->SetKeyboardFocus();
    bCameraInputModeInitialized = false;
}

void APGPlayerController::CloseSettings()
{
    if (!SettingsWidget) return;
    if (auto* UI = UPGUIManager::Get(this)) UI->ReleaseModalInput(SettingsWidget);
    SettingsWidget->RemoveFromParent();
    SettingsWidget = nullptr;
    RefreshCameraInputMode();
}

void APGPlayerController::SetPreferredCameraMode(EPGCameraMode Mode)
{
    if (!IsLocalController()) return;
    auto* Settings = GetMutableDefault<UPGCameraSettings>();
    Settings->CameraMode = Mode == EPGCameraMode::Action3D ? Mode : EPGCameraMode::QuarterView;
    Settings->SaveConfig();
    if (auto* LocalCharacter = Cast<APGCharacterPlayer>(GetPawn())) LocalCharacter->SetCameraMode(Settings->GetCameraMode());
    bCameraInputModeInitialized = false;
    RefreshCameraInputMode();
}

void APGPlayerController::RefreshCameraInputMode()
{
    if (!IsLocalController()) return;
    if (const auto* UI = UPGUIManager::Get(this); UI && UI->IsWindowOpen())
    {
        bCameraInputModeInitialized = false;
        return;
    }
    const auto* LocalCharacter = Cast<APGCharacterPlayer>(GetPawn());
    const bool bCapture = LocalCharacter && LocalCharacter->GetCameraMode() == EPGCameraMode::Action3D
        && !IsInputKeyDown(EKeys::LeftAlt) && !IsInputKeyDown(EKeys::RightAlt) && !IsPaused();
    if (bCameraInputModeInitialized && bCapture == bActionMouseCaptured) return;
    bCameraInputModeInitialized = true;
    bActionMouseCaptured = bCapture;
    bShowMouseCursor = !bCapture;
    if (bCapture)
    {
        FInputModeGameOnly Input;
        Input.SetConsumeCaptureMouseDown(false);
        SetInputMode(Input);
    }
    else
    {
        FInputModeGameAndUI Input;
        Input.SetHideCursorDuringCapture(false);
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Input);
    }
}

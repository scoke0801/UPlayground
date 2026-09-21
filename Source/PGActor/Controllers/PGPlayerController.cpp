// Fill out your copyright notice in the Description page of Project Settings.

#include "PGPlayerController.h"
#include "PGUI/Widget/Window/PGUIInventory.h"
#include "PGUI/Widget/HUD/PGUIMainHUD.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
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
}

void APGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
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

	// 마우스 오버 체크 활성화 시
	if (bEnableMouseOverCheck)
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
    if (!FSlateApplication::IsInitialized()) return false;
    FSlateApplication& Slate = FSlateApplication::Get();
    const FWidgetPath Path = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows());
    for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
    {
        const FName Type = Path.Widgets[Index].Widget->GetType();
        if (Type == FName(TEXT("SObjectWidget")) || Type == FName(TEXT("SButton"))) return true;
    }
    return false;
}
void APGPlayerController::ToggleInventory()
{
    if (InventoryWidget)
    {
        InventoryWidget->RemoveFromParent(); InventoryWidget = nullptr;
        SetPause(false);
        SetIgnoreMoveInput(false); FlushPressedKeys(); return;
    }
    auto* LocalCharacter = Cast<APGCharacterPlayer>(GetPawn());
    if (!LocalCharacter || !LocalCharacter->IsGameplayInputAllowed()) return;
    LocalCharacter->GetPGAbilitySystemComponent()->CancelAllAbilities();
    LocalCharacter->GetPGAbilitySystemComponent()->ClearBufferedInput();
    InventoryWidget = CreateWidget<UPGUIInventory>(this);
    InventoryWidget->SetDesiredSizeInViewport(FVector2D(1040,900));
    InventoryWidget->SetAnchorsInViewport(FAnchors(.5f,.5f));
    InventoryWidget->SetAlignmentInViewport(FVector2D(.5f,.5f));
    InventoryWidget->AddToViewport(90);
    SetIgnoreMoveInput(true); FlushPressedKeys();
    SetPause(true);
}
void APGPlayerController::PickupNearest()
{
    auto* LocalCharacter = Cast<APGCharacterPlayer>(GetPawn());
    if (!LocalCharacter || !LocalCharacter->IsGameplayInputAllowed()) return;
    APGLootDrop* Nearest = nullptr; double Best = TNumericLimits<double>::Max();
    for (TActorIterator<APGLootDrop> It(GetWorld()); It; ++It)
    {
        const double D = FVector::DistSquared(LocalCharacter->GetActorLocation(), It->GetActorLocation());
        if (D < Best) { Best = D; Nearest = *It; }
    }
    if (Nearest) Nearest->TryPickup(LocalCharacter);
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

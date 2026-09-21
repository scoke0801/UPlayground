// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterPlayer.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Input/PGQuarterViewData.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "PGUI/Manager/PGUIManager.h"

#include "EnhancedInputSubsystems.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGActor/Components/Combat/PGPlayerCombatComponent.h"
#include "PGActor/Components/Combat/PGSkillMontageController.h"
#include "PGActor/Components/Input/PGInputComponent.h"
#include "PGActor/Components/Stat/PGPlayerStatComponent.h"
#include "PGActor/Handler/Skill/PGPlayerSkillHandler.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/DataAsset/StartUpData/PGDataAsset_StartUpDataBase.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Debug/PGDebugHelper.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGShared/Shared/Message/Stat/PGStatUpdateEventData.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"
#include "PGUI/Component/Base/PGWidgetComponentBase.h"
#include "PGUI/Manager/PGDamageFloaterManager.h"
#include "PGUI/Widget/Billboard/PGUIPlayerHpBar.h"

APGCharacterPlayer::APGCharacterPlayer()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->SocketOffset = FVector(0.f,0.f, 55.f);
	CameraBoom->bUsePawnControlRotation = true;  // 컨트롤러 회전에 따라 SpringArm 회전
	CameraBoom->bInheritPitch = true;           // 피치 회전 허용
	CameraBoom->bInheritYaw = true;             // 요 회전 허용
	CameraBoom->bInheritRoll = false;           // 롤 회전 비허용
	
	//CameraBoom->bEnableCameraLag = true;
	//CameraBoom->CameraLagSpeed = 3.0f;
	//CameraBoom->bEnableCameraRotationLag = true;
	//CameraBoom->CameraRotationLagSpeed = 8.0f;
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;	// 카메라 자체는 회전 안함
	
	GetCharacterMovement()->bOrientRotationToMovement = false;  // 수동 회전 제어
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 100.f;

	CombatComponent = CreateDefaultSubobject<UPGPlayerCombatComponent>(TEXT("PlayerCombatComponent"));
	PlayerStatComponent = CreateDefaultSubobject<UPGPlayerStatComponent>(TEXT("PlayerStatComponent"));
	PlayerHpWidgetComponent = CreateDefaultSubobject<UPGWidgetComponentBase>(TEXT("PlayerHpWidget"));
	if (PlayerHpWidgetComponent)
	{
		PlayerHpWidgetComponent->SetupAttachment(GetCapsuleComponent());
		// 캡슐의 Half Height 가져오기
		float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		FVector BottomPosition = FVector(0.0f, 0.0f, -CapsuleHalfHeight);
		PlayerHpWidgetComponent->SetRelativeLocation(BottomPosition);

		PlayerHpWidget = Cast<UPGUIPlayerHpBar>(PlayerHpWidgetComponent->GetWidget());
	}
	
	SkillMontageController = CreateDefaultSubobject<UPGSkillMontageController>(TEXT("SkillMontageController"));
}

void APGCharacterPlayer::BeginPlay()
{
	Super::BeginPlay();
    if (auto* Profile = UPGProfileSubsystem::Get(this)) Profile->RestorePlayer(this);
    AbilitySystemComponent->RestoreHealth(AbilitySystemComponent->GetCombatStat(EPGStatType::Health));
    ConfigureQuarterView();

	InitUIComponents();

	bIsAllMeshLoaded = false;
	CheckAllMeshesLoaded();
	
	if (false == bIsAllMeshLoaded)
	{
		GetWorldTimerManager().SetTimer(MeshCheckTimerHandle, this, &ThisClass::CheckAllMeshesLoaded, 0.1f, true);
	}
	//UPGMessageManager::Get(this)->SendMessage(EPGPlayerMessageType::Spawned, nullptr);
}

void APGCharacterPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	APlayerController* PC = GetController<APlayerController>();
    if (!PC || !InputConfigDataAsset) return;
    ULocalPlayer* localPlayer = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(localPlayer);
	if (!Subsystem)
	{
		return;
	}

	Subsystem->AddMappingContext(InputConfigDataAsset->DefaultMappingContext, 0);
	UPGInputComponent* PgInputComponent = CastChecked<UPGInputComponent>(PlayerInputComponent);
	
	PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Move,
		ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Look,
		ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Zoom,
		ETriggerEvent::Triggered, this, &ThisClass::Input_Zoom);

	PgInputComponent->BindAbilityInputAction(InputConfigDataAsset, this,
		&ThisClass::Input_AbilityInputPressed, &ThisClass::input_AbilityInputReleased);
}

void APGCharacterPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
    if (SkillHandler) return;
	
	if (false == CharacterStartUpData.IsNull())
	{
		if (UPGDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.LoadSynchronous())
		{
			LoadedData->GiveToAbilitySystemComponent(AbilitySystemComponent);
		}
	}

	if (SkillHandler) return;
    SkillHandler = FPGHandler::Create<FPGPlayerSkillHandler>();
    SkillHandler->SetContext(this);
    if (auto* Profile = UPGProfileSubsystem::Get(this)) Profile->RestorePlayer(this);
}

void APGCharacterPlayer::OnHit(UPGStatComponent* Source, const UPGPawnCombatComponent* Combat)
{
    EPGDamageType Type = EPGDamageType::Normal;
    const float Damage = AbilitySystemComponent->ReceiveCombatHit(Source ? Source->GetASC() : nullptr, Type);
    if (Damage > 0.f)
    {
        if (auto* Manager = UPGDamageFloaterManager::Get(this)) Manager->AddFloater(FMath::RoundToInt(Damage), Type, this, true);
        PlayCombatFeedback(Source ? Source->GetOwner() : nullptr, Type);
    }
}

void APGCharacterPlayer::StartSkillWindow()
{
    bSkillWindowOpen = true;
}

void APGCharacterPlayer::EndSkillWindow()
{
    bSkillWindowOpen = false;
}

void APGCharacterPlayer::SetIsJump(bool IsJump)
{
	bIsJump = IsJump;
}

bool APGCharacterPlayer::IsCanJump() const
{
	if (!IsGameplayInputAllowed())
	{
		return false;
	}

	if (UPawnMovementComponent* MovementComponent = GetMovementComponent())
	{
		return !MovementComponent->IsFalling();
	}
	
	return false;
}

void APGCharacterPlayer::CheckAllMeshesLoaded()
{
	if (true == bIsAllMeshLoaded)
	{
		return;
	}
	
	// 모든 스켈레탈 메시 컴포넌트 수집
	TArray<USkeletalMeshComponent*> MeshComponents;
	GetComponents<USkeletalMeshComponent>(MeshComponents);
    
	if (MeshComponents.Num() == 0)
	{
		return;
	}
    
	// 각 컴포넌트의 메시 로딩 상태 확인
	int32 LoadedCount = 0;
	int32 TotalCount = MeshComponents.Num();
    
	for (USkeletalMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp && MeshComp->GetSkeletalMeshAsset())
		{
			LoadedCount++;
		}
		else if (MeshComp)
		{
			UE_LOG(LogTemp, Log, TEXT("Mesh not loaded yet: %s"), *MeshComp->GetName());
		}
	}
	bIsAllMeshLoaded = LoadedCount == TotalCount;
	// 모든 메시 로딩 완료 확인
	if (true == bIsAllMeshLoaded)
	{
		UE_LOG(LogTemp, Log, TEXT("All %d meshes loaded successfully!"), TotalCount);
		// 타이머 정리
		if (MeshCheckTimerHandle.IsValid())
		{
			GetWorldTimerManager().ClearTimer(MeshCheckTimerHandle);
		}
        
		UE_LOG(LogTemp, Log, TEXT("All %d meshes loaded successfully!"), TotalCount);
        
		// 델리게이트 브로드캐스트
		UPGMessageManager::Get(this)->SendMessage(EPGPlayerMessageType::Spawned, nullptr);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Meshes loading... %d/%d"), LoadedCount, TotalCount);
	}
}

void APGCharacterPlayer::Input_Move(const FInputActionValue& InputActionValue)
{
	if (!IsGameplayInputAllowed())
	{
		return;
	}
	
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();

	if (MovementVector.SizeSquared() > 0.1f)
	{
		// 카메라(컨트롤러) 방향 기준으로 이동
		const FRotator ControlRotation = bUseQuarterView ? CameraBoom->GetComponentRotation() : Controller->GetControlRotation();
		const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
        
		const FVector ForwardDirection = YawRotation.RotateVector(FVector::ForwardVector);
		const FVector RightDirection = YawRotation.RotateVector(FVector::RightVector);
		const FVector MovementDirection = (ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X).GetSafeNormal();
        
		AddMovementInput(MovementDirection, 1.0f);
        
		// 이동 방향으로 캐릭터 회전 (카메라는 독립적으로 공전)
		const FRotator TargetRotation = MovementDirection.Rotation();
		const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, GetWorld()->GetDeltaSeconds(), 8.0f);
		if (!bUseQuarterView) SetActorRotation(NewRotation);
	}
}

void APGCharacterPlayer::Input_Look(const FInputActionValue& InputActionValue)
{
    if (bUseQuarterView || !IsGameplayInputAllowed()) return;
	const FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();

   
	if (Controller != nullptr)
	{
		// 현재 Control Rotation 가져오기
		FRotator CurrentRotation = GetControlRotation();
        
		// 새로운 Pitch 계산
		float NewPitch = CurrentRotation.Pitch + (LookAxisVector.Y * -1.0f * MouseSensitivityY); // Y축 반전
        
		// Pitch 제한 적용
		NewPitch = FMath::ClampAngle(NewPitch, CameraMinPitch, CameraMaxPitch);
        
		// Yaw는 제한 없이
		float NewYaw = CurrentRotation.Yaw + LookAxisVector.X * MouseSensitivityX;
        
		// 새로운 Rotation 설정
		FRotator NewRotation = FRotator(NewPitch, NewYaw, 0.0f);
		Controller->SetControlRotation(NewRotation);
	}
	
	// if (0.f != LookAxisVector.X)
	// {
	// 	AddControllerYawInput(LookAxisVector.X * MouseSensitivityX);  // 좌우 회전
	// }
	//
	// if (0.f != LookAxisVector.Y)
	// {
	// 	AddControllerPitchInput(LookAxisVector.Y * MouseSensitivityY); // 상하 회전
	// }
}

void APGCharacterPlayer::Input_Zoom(const FInputActionValue& InputActionValue)
{
    if (!IsGameplayInputAllowed()) return;
	const float Delta = InputActionValue.Get<float>();

	float NewLength = FMath::Clamp(CameraBoom->TargetArmLength - Delta * CameraUpdateSpeed, CameraMinOffset, CameraMaxOffset);
	CameraBoom->TargetArmLength = NewLength;
}

void APGCharacterPlayer::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
	if (!IsGameplayInputAllowed()) { AbilitySystemComponent->ClearBufferedInput(); return; }
    if (const APGPlayerController* PC = Cast<APGPlayerController>(Controller))
        if (PC->IsPointerOverUI()) return;
    FaceAimDirection();
    AbilitySystemComponent->OnAbilityInputPressed(InInputTag);
}

void APGCharacterPlayer::input_AbilityInputReleased(FGameplayTag InInputTag)
{
	AbilitySystemComponent->OnAbilityInputReleased(InInputTag);
}
UPGPawnCombatComponent* APGCharacterPlayer::GetCombatComponent() const
{
	return CombatComponent;
}

UPGStatComponent* APGCharacterPlayer::GetStatComponent() const
{
	return PlayerStatComponent;
}

UPGPlayerStatComponent* APGCharacterPlayer::GetPlayerStatComponent() const
{
	return PlayerStatComponent;
}

void APGCharacterPlayer::InitUIComponents()
{
	if (PlayerHpWidgetComponent)
	{
		PlayerHpWidget = Cast<UPGUIPlayerHpBar>(PlayerHpWidgetComponent->GetWidget());
	}
	
	UpdateHpComponent();	
}

void APGCharacterPlayer::UpdateHpComponent()
{
	if (nullptr == PlayerHpWidget)
	{
		return;
	}
	PlayerHpWidget->SetHpPercent(PlayerStatComponent->GetHealthRatio());
}

void APGCharacterPlayer::OnHealthChanged()
{
    const bool bWasDead = bDeathStarted;
    Super::OnHealthChanged();
    if (!bWasDead && bDeathStarted && UPGMessageManager::Get(this)) UPGMessageManager::Get(this)->SendMessage(EPGPlayerMessageType::Died, nullptr);
    if (GetStatComponent()->GetCurrentHealth() <= 0.f) bIsCanControl = false;
    if (auto* Manager = UPGMessageManager::Get(this))
    {
        FPGStatUpdateEventData Data(EPGStatType::Health, FMath::RoundToInt(PlayerStatComponent->GetCurrentHealth()), PlayerStatComponent->GetStat(EPGStatType::Health));
        Manager->SendMessage(EPGPlayerMessageType::StatUpdate, &Data);
    }
    UpdateHpComponent();
}
void APGCharacterPlayer::ConfigureQuarterView()
{
    if (!bUseQuarterView) return;
    const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = CameraBoom->bInheritYaw = CameraBoom->bInheritRoll = false;
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->SetWorldRotation(Data->Rotation);
    CameraMinOffset = FMath::Max(100.f, Data->MinDistance);
    CameraMaxOffset = FMath::Max(CameraMinOffset, Data->MaxDistance);
    CameraBoom->TargetArmLength = FMath::Clamp(Data->Distance, CameraMinOffset, CameraMaxOffset);
    CameraBoom->bEnableCameraLag = Data->LagSpeed > 0.f;
    CameraBoom->CameraLagSpeed = FMath::Max(0.f, Data->LagSpeed);
    CameraUpdateSpeed = Data->ZoomSpeed;
    LastAimDirection = GetActorForwardVector();
    GetWorldTimerManager().SetTimer(AimTimer, this, &ThisClass::UpdateAim, 1.f / 60.f, true);
}
bool APGCharacterPlayer::IsGameplayInputAllowed() const
{
    const APlayerController* PC = Cast<APlayerController>(Controller);
    if (!bIsCanControl || !PC || PC->IsMoveInputIgnored() || GetWorld()->IsPaused() || bDeathStarted) return false;
    if (UPGUIManager::Get(this) && UPGUIManager::Get(this)->IsWindowOpen()) return false;
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    return !Viewport || !Viewport->Viewport || Viewport->Viewport->HasFocus();
}
void APGCharacterPlayer::UpdateAim()
{
    if (!IsGameplayInputAllowed()) { AbilitySystemComponent->ClearBufferedInput(); return; }
    APGPlayerController* PC = Cast<APGPlayerController>(Controller);
    if (!PC || PC->IsPointerOverUI()) return;
    FVector Origin, Direction;
    if (PC->DeprojectMousePositionToWorld(Origin, Direction))
    {
        const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PGQuarterViewAim), false, this);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * Data->AimTraceDistance, Data->AimChannel, Params))
        {
            const FVector Aim = (Hit.ImpactPoint - GetActorLocation()).GetSafeNormal2D();
            if (!Aim.IsNearlyZero()) LastAimDirection = Aim;
        }
    }
    if (!GetMesh()->GetAnimInstance() || !GetMesh()->GetAnimInstance()->IsAnyMontagePlaying()) FaceAimDirection();
}
void APGCharacterPlayer::FaceAimDirection()
{
    if (bUseQuarterView && !LastAimDirection.IsNearlyZero() && !bDeathStarted) SetActorRotation(LastAimDirection.Rotation());
}
bool APGCharacterPlayer::CanStartSkill(bool bDodge) const
{
    if (!IsGameplayInputAllowed()) return false;
    const UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (!Anim) return false;
    const UAnimMontage* Montage = Anim->GetCurrentActiveMontage();
    if (!Montage || bSkillWindowOpen) return true;
    const float Length = Montage->GetPlayLength();
    const float Remaining = Length > 0.f ? (Length - Anim->Montage_GetPosition(Montage)) / Length : 0.f;
    return Remaining <= (bDodge ? DodgeCancelFraction : AttackCancelFraction);
}
void APGCharacterPlayer::SetSkillCancelPolicy(float AttackFraction, float DodgeFraction)
{
    bSkillWindowOpen = false;
    AttackCancelFraction = FMath::Clamp(AttackFraction, 0.f, 1.f);
    DodgeCancelFraction = FMath::Clamp(DodgeFraction, 0.f, 1.f);
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGConsumableComponent.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGActor/Components/Combat/PGPlayerDashComponent.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Input/PGQuarterViewData.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "PGUI/Manager/PGUIManager.h"

#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "PGData/DataAsset/Input/PGPlayerLocomotionData.h"
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
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"
#include "PGUI/Component/Base/PGWidgetComponentBase.h"
#include "PGUI/Manager/PGDamageFloaterManager.h"

APGCharacterPlayer::APGCharacterPlayer()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    LocomotionDataAsset = TSoftObjectPtr<UPGPlayerLocomotionData>(FSoftObjectPath(TEXT("/Game/DataCenter/PlayerTurns/DA_PlayerLocomotion.DA_PlayerLocomotion")));
    ConsumableComponent = CreateDefaultSubobject<UPGConsumableComponent>(TEXT("ConsumableComponent"));
    PlayerDashComponent = CreateDefaultSubobject<UPGPlayerDashComponent>(TEXT("PlayerDashComponent"));
    PlayerAttackComponent = CreateDefaultSubobject<UPGPlayerAttackComponent>(TEXT("PlayerAttackComponent"));
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
        // Retain the named subobject for existing Blueprint compatibility.
        PlayerHpWidgetComponent->SetVisibility(false);
        PlayerHpWidgetComponent->SetHiddenInGame(true);
        PlayerHpWidgetComponent->SetComponentTickEnabled(false);
	}
	
	SkillMontageController = CreateDefaultSubobject<UPGSkillMontageController>(TEXT("SkillMontageController"));
}

void APGCharacterPlayer::BeginPlay()
{
	Super::BeginPlay();
    if (auto* Profile = UPGProfileSubsystem::Get(this)) Profile->RestorePlayer(this);
    PlayerAttackComponent->PrepareLoadout();
    PlayerDashComponent->PrepareAfterimages();
    AbilitySystemComponent->RestoreHealth(AbilitySystemComponent->GetCombatStat(EPGStatType::Health));
    const auto* ConsumableProfile = UPGProfileSubsystem::Get(this);
    ConsumableComponent->Initialize(ConsumableProfile && ConsumableProfile->GetCatalog() ? ConsumableProfile->GetCatalog()->HealingPotion.Get() : nullptr);
    ConfigureQuarterView();
    LocomotionData = LocomotionDataAsset.LoadSynchronous();

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
    PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Move,
        ETriggerEvent::Completed, this, &ThisClass::Input_MoveReleased);
    PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Move,
        ETriggerEvent::Canceled, this, &ThisClass::Input_MoveReleased);
	PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Look,
		ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Zoom,
		ETriggerEvent::Triggered, this, &ThisClass::Input_Zoom);
    PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_HealingPotion,
        ETriggerEvent::Started, this, &ThisClass::Input_HealingPotion);

	PgInputComponent->BindAbilityInputAction(InputConfigDataAsset, this,
		&ThisClass::Input_AbilityInputPressed, &ThisClass::input_AbilityInputReleased, &ThisClass::Input_AbilityInputHeld);
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
    PlayerAttackComponent->PrepareLoadout();
    // Spawned players can BeginPlay before possession; equip after startup abilities are granted.
    if (!CombatComponent->GetCharacterCurrentEquippedWeapon())
    {
        FGameplayAbilitySpecHandle EquipHandle;
        for (const auto& Spec : AbilitySystemComponent->GetActivatableAbilities())
            if (Spec.GetDynamicSpecSourceTags().HasTagExact(PGGamePlayTags::InputTag_Equip_Weapon))
            { EquipHandle = Spec.Handle; break; }
        if (!EquipHandle.IsValid() || !AbilitySystemComponent->TryActivateAbility(EquipHandle))
            UE_LOG(LogTemp, Warning, TEXT("Player startup weapon could not be equipped. Check StartUpData."));
    }
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
    MoveInputDirection = FVector::ZeroVector;
	if (!IsGameplayInputAllowed())
	{
		CancelLocomotionTurn();
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
        MoveInputDirection = MovementDirection;
        
		AddMovementInput(MovementDirection, UpdateMovementFacing(MovementDirection, GetWorld()->GetDeltaSeconds()));
        bCanStartStationaryTurn = false;
	}
    else { CancelLocomotionTurn(); bCanStartStationaryTurn = true; }
}

void APGCharacterPlayer::CancelLocomotionTurn()
{
    if (bLocomotionTurning) TurnCooldown = GetWorld()->GetTimeSeconds() + (LocomotionData ? LocomotionData->BlendOutSeconds : .12f);
    bLocomotionTurning = false;
    // Retain the last pose while the graph blends back to directional locomotion.
}

float APGCharacterPlayer::UpdateMovementFacing(const FVector& Direction, float DeltaSeconds)
{
    const UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (PlayerAttackComponent->IsRunning() && LocomotionData)
        CombatFacingUntil = GetWorld()->GetTimeSeconds() + LocomotionData->CombatStrafeSeconds;
    if (bTrackAttackAim || PlayerAttackComponent->IsRunning() || PlayerDashComponent->IsDashing() ||
        GetCharacterMovement()->IsFalling() || (Anim && Anim->IsAnyMontagePlaying()))
    {
        CancelLocomotionTurn();
        return 1.f;
    }
    // Actual local velocity selects left/right/back (including diagonals) in BS_PlayerSword.
    // Do not rotate that direction back to zero while chaining combat movement.
    if (GetWorld()->GetTimeSeconds() < CombatFacingUntil)
    {
        CancelLocomotionTurn();
        return 1.f;
    }
    const float TargetYaw = Direction.Rotation().Yaw;
    float DeltaYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, TargetYaw);
    // Exact reversals have two equally short paths; keep their choice stable under tiny input noise.
    if (FMath::Abs(DeltaYaw) > 179.f) DeltaYaw = 180.f * LastTurnSign;
    if (bLocomotionTurning && FMath::Abs(FMath::FindDeltaAngleDegrees(TurnStartYaw + TurnAngle, TargetYaw)) > LocomotionData->RetargetCancelAngle)
        CancelLocomotionTurn();

    // Absolute time also expires the blend-out guard while input is released or combat owns facing.
    if (!bLocomotionTurning && GetWorld()->GetTimeSeconds() >= TurnCooldown && LocomotionData &&
        (LocomotionData->bAllowMovingTurns ||
            (bCanStartStationaryTurn && GetVelocity().Size2D() <= LocomotionData->StationarySpeed)))
    {
        const float Threshold = GetVelocity().Size2D() <= LocomotionData->StationarySpeed
            ? LocomotionData->StartTurnAngle : LocomotionData->MovingPivotAngle;
        if (FMath::Abs(DeltaYaw) >= Threshold)
        {
            float BestError = MAX_flt;
            int32 BestIndex = INDEX_NONE;
            for (int32 Index = 0; Index < LocomotionData->Turns.Num(); ++Index)
            {
                const auto& Turn = LocomotionData->Turns[Index];
                if (!Turn.Animation || Turn.Animation->GetPlayLength() <= SMALL_NUMBER || Turn.RotationProgress.Num() < 2 || Turn.Angle * DeltaYaw <= 0.f) continue;
                const float Error = FMath::Abs(Turn.Angle - DeltaYaw);
                if (Error < BestError) { BestError = Error; BestIndex = Index; }
            }
            if (BestIndex != INDEX_NONE)
            {
                TurnMotionIndex = BestIndex;
                TurnAnimation = LocomotionData->Turns[BestIndex].Animation;
                TurnAnimationTime = 0.f;
                TurnEffectivePlayRate = FMath::Max(FMath::Max(.1f, LocomotionData->TurnPlayRate),
                    TurnAnimation->GetPlayLength() / FMath::Max(.1f, LocomotionData->MaxTurnSeconds));
                TurnStartYaw = GetActorRotation().Yaw;
                TurnAngle = DeltaYaw;
                LastTurnSign = FMath::Sign(DeltaYaw);
                bLocomotionTurning = true;
            }
        }
    }
    if (bLocomotionTurning)
    {
        const auto& Turn = LocomotionData->Turns[TurnMotionIndex];
        TurnAnimationTime = FMath::Min(TurnAnimationTime + DeltaSeconds * TurnEffectivePlayRate, TurnAnimation->GetPlayLength());
        const float Fraction = TurnAnimationTime / TurnAnimation->GetPlayLength();
        const float Sample = Fraction * (Turn.RotationProgress.Num() - 1);
        const int32 Index = FMath::Min(FMath::FloorToInt(Sample), Turn.RotationProgress.Num() - 2);
        const float Progress = FMath::Lerp(Turn.RotationProgress[Index], Turn.RotationProgress[Index + 1], Sample - Index);
        SetActorRotation(FRotator(0.f, TurnStartYaw + TurnAngle * Progress, 0.f));
        const float Resume = FMath::Clamp(LocomotionData->MovementResumeFraction, 0.f, .95f);
        // Hand back to the directional gait as acceleration resumes, rather than
        // translating a full-weight planted turn pose across the floor.
        const float MovementScale = FMath::SmoothStep(Resume, FMath::Min(Resume + .25f, 1.f), Fraction);
        if (Fraction >= 1.f) CancelLocomotionTurn();
        return MovementScale;
    }
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, TargetYaw, 0.f), DeltaSeconds, MovementFacingInterpSpeed));
    return 1.f;
}

void APGCharacterPlayer::Input_MoveReleased(const FInputActionValue& InputActionValue)
{
    MoveInputDirection = FVector::ZeroVector;
    bCanStartStationaryTurn = true;
    CancelLocomotionTurn();
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
    const APGPlayerController* PC = Cast<APGPlayerController>(Controller);
    if (PC && PC->IsPointerOverUI()) return;
    const float Delta = InputActionValue.Get<float>();
    if (bUseQuarterView)
        TargetCameraDistance = FMath::Clamp(TargetCameraDistance - Delta * CameraUpdateSpeed, CameraMinOffset, CameraMaxOffset);
    else
        CameraBoom->TargetArmLength = FMath::Clamp(CameraBoom->TargetArmLength - Delta * CameraUpdateSpeed, CameraMinOffset, CameraMaxOffset);
}

void APGCharacterPlayer::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
	if (InInputTag == PGGamePlayTags::InputTag_Equip_Weapon || InInputTag == PGGamePlayTags::InputTag_UnEquip_Weapon) return;
	if (!IsGameplayInputAllowed()) { AbilitySystemComponent->ClearBufferedInput(); return; }
    if (const APGPlayerController* PC = Cast<APGPlayerController>(Controller))
        if (PC->IsPointerOverUI()) return;
    AbilitySystemComponent->OnAbilityInputPressed(InInputTag);
}

void APGCharacterPlayer::Input_AbilityInputHeld(FGameplayTag InInputTag)
{
    if (InInputTag != PGGamePlayTags::InputTag_Skill_Normal) return;
    const auto* PC = Cast<APGPlayerController>(Controller);
    if (!IsGameplayInputAllowed() || (PC && PC->IsPointerOverUI()))
    {
        AbilitySystemComponent->ClearBufferedInput();
        return;
    }
    AbilitySystemComponent->OnAbilityInputHeld(InInputTag);
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
        // Enforce after Blueprint defaults, including on respawn. Health lives in the HUD.
        PlayerHpWidgetComponent->SetVisibility(false);
        PlayerHpWidgetComponent->SetHiddenInGame(true);
        PlayerHpWidgetComponent->SetWidget(nullptr);
        PlayerHpWidgetComponent->SetComponentTickEnabled(false);
    }
}

void APGCharacterPlayer::OnHealthChanged()
{
    const bool bWasDead = bDeathStarted;
    Super::OnHealthChanged();
    if (!bWasDead && bDeathStarted) ConsumableComponent->OnOwnerDied();
    if (!bWasDead && bDeathStarted && UPGMessageManager::Get(this)) UPGMessageManager::Get(this)->SendMessage(EPGPlayerMessageType::Died, nullptr);
    if (GetStatComponent()->GetCurrentHealth() <= 0.f) bIsCanControl = false;
    if (auto* Manager = UPGMessageManager::Get(this))
    {
        FPGStatUpdateEventData Data(EPGStatType::Health, FMath::RoundToInt(PlayerStatComponent->GetCurrentHealth()), PlayerStatComponent->GetStat(EPGStatType::Health));
        Manager->SendMessage(EPGPlayerMessageType::StatUpdate, &Data);
    }
}
void APGCharacterPlayer::ConfigureQuarterView()
{
    if (!bUseQuarterView) return;
    const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = CameraBoom->bInheritYaw = CameraBoom->bInheritRoll = false;
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->SetWorldRotation(FRotator(Data->Rotation.Pitch, Data->Rotation.Yaw, 0.f));
    CameraMinOffset = FMath::Clamp(Data->CloseUpDistance, 100.f, FMath::Max(100.f, Data->MinDistance));
    CameraMaxOffset = FMath::Max(CameraMinOffset, Data->MaxDistance);
    CameraBoom->TargetArmLength = FMath::Clamp(Data->Distance, CameraMinOffset, CameraMaxOffset);
    CameraBoom->bEnableCameraLag = Data->LagSpeed > 0.f;
    CameraBoom->CameraLagSpeed = FMath::Max(0.f, Data->LagSpeed);
    // Blueprint defaults must not disable world collision for either player view.
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->ProbeChannel = ECC_Camera;
    CameraBoom->ProbeSize = FMath::Max(12.f, Data->CameraProbeRadius);
    CameraBoom->bEnableCameraRotationLag = false;
    CameraBoom->bUseCameraLagSubstepping = true;
    CameraBoom->CameraLagMaxTimeStep = 1.f / 60.f;
    CameraBoom->CameraLagMaxDistance = FMath::Max(0.f, Data->CameraMaxLagDistance);
    // Keep the view at the swept socket; a child offset bypasses collision protection.
    FollowCamera->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    CameraUpdateSpeed = FMath::Max(1.f, Data->ZoomSpeed);
    TargetCameraDistance = CameraBoom->TargetArmLength;
    CameraPitchOffset = 0.f;
    CombatCameraTargetOffset = CameraBoom->TargetOffset;
    CameraBoom->AddTickPrerequisiteActor(this);
    SetActorTickEnabled(true);
    QuarterViewDistance = TargetCameraDistance;
    ActionCameraDistance = Data->ActionDistance;
    ActionCameraRotation = FRotator(Data->ActionPitch, GetActorRotation().Yaw, 0.f);
    SetCameraMode(GetDefault<UPGCameraSettings>()->GetCameraMode());
    UpdatePlayerCamera(0.f);
    LastAimDirection = GetActorForwardVector();
    GetWorldTimerManager().SetTimer(AimTimer, this, &ThisClass::UpdateAim, 1.f / 60.f, true);
}
void APGCharacterPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bUseQuarterView || !IsLocallyControlled()) return;
    UpdatePlayerCamera(DeltaSeconds);
}

void APGCharacterPlayer::SetCameraMode(EPGCameraMode Mode)
{
    Mode = Mode == EPGCameraMode::Action3D ? Mode : EPGCameraMode::QuarterView;
    if (CameraMode == Mode) return;
    const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
    if (CameraMode == EPGCameraMode::QuarterView) QuarterViewDistance = TargetCameraDistance;
    else ActionCameraDistance = TargetCameraDistance;
    CameraMode = Mode;
    if (Mode == EPGCameraMode::Action3D)
    {
        CameraMinOffset = FMath::Max(100.f, Data->ActionMinDistance);
        CameraMaxOffset = FMath::Max(CameraMinOffset, Data->ActionMaxDistance);
        TargetCameraDistance = FMath::Clamp(ActionCameraDistance, CameraMinOffset, CameraMaxOffset);
    }
    else
    {
        CameraMinOffset = FMath::Clamp(Data->CloseUpDistance, 100.f, FMath::Max(100.f, Data->MinDistance));
        CameraMaxOffset = FMath::Max(CameraMinOffset, Data->MaxDistance);
        TargetCameraDistance = FMath::Clamp(QuarterViewDistance, CameraMinOffset, CameraMaxOffset);
    }
    // Do not blend through a wall or carry a ground target from the previous view.
    CameraBoom->TargetArmLength = TargetCameraDistance;
    bHasAimPoint = false;
    UpdatePlayerCamera(0.f);
}

float APGCharacterPlayer::GetCameraBodyFade(const FVector& ViewLocation, const UCapsuleComponent* Body) const
{
    if (!bUseQuarterView || CameraMode != EPGCameraMode::Action3D) return 0.f;
    const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
    const UCapsuleComponent* Capsule = Body ? Body : GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const FVector Axis = Capsule->GetUpVector() * FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
    const FVector Center = Capsule->GetComponentLocation();
    const float Distance = FMath::Sqrt(FMath::PointDistToSegmentSquared(ViewLocation, Center - Axis, Center + Axis));
    const float Inner = Radius + FMath::Max(0.f, Data->CameraBodyClearance);
    return 1.f - FMath::SmoothStep(Inner, Inner + FMath::Max(1.f, Data->CameraBodyFadeDistance), Distance);
}

bool APGCharacterPlayer::ShouldHideForCamera(const FVector& ViewLocation, bool bWasHidden) const
{
    if (!bUseQuarterView || CameraMode != EPGCameraMode::Action3D) return false;
    const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const FVector Axis = Capsule->GetUpVector() * FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
    const FVector Center = Capsule->GetComponentLocation();
    const float Clearance = Radius + FMath::Max(0.f, Data->CameraBodyClearance)
        + (bWasHidden ? FMath::Max(0.f, Data->CameraBodyHideHysteresis) : 0.f);
    return FMath::PointDistToSegmentSquared(ViewLocation, Center - Axis, Center + Axis) < FMath::Square(Clearance);
}

void APGCharacterPlayer::UpdatePlayerCamera(float DeltaSeconds)
{
    const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetCameraDistance,
        DeltaSeconds, FMath::Max(.1f, Data->ZoomInterpSpeed));
    if (CameraMode == EPGCameraMode::Action3D)
    {
        const float MinPitch = FMath::Clamp(Data->ActionMinPitch, -89.f, 89.f);
        const float MaxPitch = FMath::Clamp(Data->ActionMaxPitch, MinPitch, 89.f);
        APGPlayerController* PC = Cast<APGPlayerController>(Controller);
        if (DeltaSeconds > 0.f && PC && !PC->bShowMouseCursor && IsGameplayInputAllowed())
        {
            float MouseX = 0.f, MouseY = 0.f;
            PC->GetInputMouseDelta(MouseX, MouseY);
            ApplyActionCameraMouseDelta(MouseX, MouseY);
        }
        ActionCameraRotation.Pitch = FMath::Clamp(ActionCameraRotation.Pitch, MinPitch, MaxPitch);
        ActionCameraRotation.Roll = 0.f;
        CameraBoom->SetWorldRotation(ActionCameraRotation);
        CameraBoom->TargetOffset = CombatCameraTargetOffset + FVector(0.f, 0.f, Data->ActionFocusHeight);
        return;
    }
    const float TransitionDistance = FMath::Clamp(Data->MinDistance, CameraMinOffset, CameraMaxOffset);
    const float CloseUpAlpha = TransitionDistance > CameraMinOffset + KINDA_SMALL_NUMBER
        ? 1.f - FMath::SmoothStep(CameraMinOffset, TransitionDistance, CameraBoom->TargetArmLength) : 0.f;
    const float BasePitch = FMath::Lerp(Data->Rotation.Pitch, Data->CloseUpPitch, CloseUpAlpha);
    const float MinPitch = FMath::Clamp(Data->MinPitch, -89.f, -5.f);
    const float MaxPitch = FMath::Clamp(Data->MaxPitch, MinPitch, -5.f);
    APGPlayerController* PC = Cast<APGPlayerController>(Controller);
    // Keep cursor aiming independent of the camera. Only middle-button vertical drag changes pitch.
    if (DeltaSeconds > 0.f && PC && IsGameplayInputAllowed() && !PC->IsPointerOverUI() && PC->IsInputKeyDown(EKeys::MiddleMouseButton))
    {
        float MouseX = 0.f, MouseY = 0.f;
        PC->GetInputMouseDelta(MouseX, MouseY);
        const float CurrentPitch = FMath::Clamp(BasePitch + CameraPitchOffset, MinPitch, MaxPitch);
        CameraPitchOffset = FMath::Clamp(CurrentPitch - MouseY * MouseSensitivityY, MinPitch, MaxPitch) - BasePitch;
    }
    CameraBoom->SetWorldRotation(FRotator(FMath::Clamp(BasePitch + CameraPitchOffset, MinPitch, MaxPitch), Data->Rotation.Yaw, 0.f));
    CameraBoom->TargetOffset = CombatCameraTargetOffset + FVector(0.f, 0.f, Data->CloseUpFocusHeight * CloseUpAlpha);
}

void APGCharacterPlayer::ApplyActionCameraMouseDelta(float MouseX, float MouseY)
{
    // Raw mouse delta is displacement: do not multiply by frame time.
    // Positive raw MouseY looks up, matching the requested reversal of action mode.
    ActionCameraRotation.Yaw = FRotator::NormalizeAxis(ActionCameraRotation.Yaw + MouseX * MouseSensitivityX);
    ActionCameraRotation.Pitch += MouseY * MouseSensitivityY;
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
    if (!IsGameplayInputAllowed()) { MoveInputDirection = FVector::ZeroVector; CancelLocomotionTurn(); AbilitySystemComponent->ClearBufferedInput(); return; }
    if (PlayerAttackComponent->IsRunning() && LocomotionData)
        CombatFacingUntil = GetWorld()->GetTimeSeconds() + LocomotionData->CombatStrafeSeconds;
    APGPlayerController* PC = Cast<APGPlayerController>(Controller);
    if (!PC || PC->IsPointerOverUI()) return;
    // Keep cursor targeting current without overriding locomotion or idle facing.
    // Attacks explicitly acquire aim on activation; legacy tracking remains opt-in.
    if (bTrackAttackAim)
        FaceAimDirection();
    else RefreshCursorAim();
}
bool APGCharacterPlayer::IsConsumableInputAllowed() const
{
    const auto* PC = Cast<APlayerController>(Controller);
    // A potion does not change attack, dash or hit-reaction control locks.
    if (!PC || !GetWorld() || GetWorld()->IsPaused() || bDeathStarted || PC->IsMoveInputIgnored()) return false;
    if (const auto* UI = UPGUIManager::Get(this); UI && UI->IsWindowOpen()) return false;
    const auto* Viewport = GetWorld()->GetGameViewport();
    return !Viewport || !Viewport->Viewport || Viewport->Viewport->HasFocus();
}
void APGCharacterPlayer::Input_HealingPotion(const FInputActionValue& InputActionValue)
{
    ConsumableComponent->TryUse();
}

void APGCharacterPlayer::RefreshCursorAim()
{
    APGPlayerController* PC = Cast<APGPlayerController>(Controller);
    if (!bUseQuarterView || !PC || PC->IsPointerOverUI()) return;
    FVector Origin, Direction;
    bool bHasRay = false;
    if (CameraMode == EPGCameraMode::Action3D && !PC->bShowMouseCursor)
    {
        int32 Width = 0, Height = 0;
        PC->GetViewportSize(Width, Height);
        bHasRay = Width > 0 && Height > 0 && PC->DeprojectScreenPositionToWorld(Width * .5f, Height * .5f, Origin, Direction);
    }
    else bHasRay = PC->DeprojectMousePositionToWorld(Origin, Direction);
    if (bHasRay)
    {
        const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PGQuarterViewAim), false, this);
        FVector AimPoint;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * Data->AimTraceDistance, Data->AimChannel, Params))
            AimPoint = Hit.ImpactPoint;
        else
        {
            // Looking into the sky in action mode must not retain an unrelated cursor target.
            if (CameraMode == EPGCameraMode::Action3D)
            {
                LastAimDirection = CameraBoom->GetForwardVector().GetSafeNormal2D();
                bHasAimPoint = false;
            }
            return;
        }
        const FVector Aim = (AimPoint - GetActorLocation()).GetSafeNormal2D();
          if (!Aim.ContainsNaN() && !Aim.IsNearlyZero()) { LastAimDirection = Aim; LastAimPoint = AimPoint; bHasAimPoint = true; }
    }
}
void APGCharacterPlayer::FaceAimDirection()
{
    CancelLocomotionTurn();
    if (LocomotionData) CombatFacingUntil = GetWorld()->GetTimeSeconds() + LocomotionData->CombatStrafeSeconds;
    RefreshCursorAim(); // Also refresh on buffered ability activation, not only the 60 Hz timer.
    if (bUseQuarterView && !LastAimDirection.IsNearlyZero() && !bDeathStarted) SetActorRotation(LastAimDirection.Rotation());
}
void APGCharacterPlayer::FaceDodgeDirection()
{
    CancelLocomotionTurn();
    if (!bDeathStarted) SetActorRotation(GetDodgeDirection().Rotation());
}
FVector APGCharacterPlayer::GetDodgeDirection()
{
    const UPGQuarterViewData* Data = QuarterViewData ? QuarterViewData.Get() : GetDefault<UPGQuarterViewData>();
    if (Data->bDodgeFollowsMovement && !MoveInputDirection.IsNearlyZero()) return MoveInputDirection.GetSafeNormal2D();
    RefreshCursorAim();
    return bUseQuarterView ? LastAimDirection.GetSafeNormal2D() : GetActorForwardVector();
}
bool APGCharacterPlayer::CanStartSkill(bool bDodge) const
{
    if (!IsGameplayInputAllowed()) return false;
    if (PlayerDashComponent->IsDashing()) return false;
    if (PlayerAttackComponent->IsRunning()) return PlayerAttackComponent->CanCancel(bDodge);
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

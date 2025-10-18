// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterPlayer.h"

#include "EnhancedInputSubsystems.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
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

	InitUIComponents();

	bIsAllMeshLoaded = false;
	CheckAllMeshesLoaded();
	
	if (false == bIsAllMeshLoaded)
	{
		GetWorldTimerManager().SetTimer(MeshCheckTimerHandle, this, &ThisClass::CheckAllMeshesLoaded, 0.1f, true);
	}
	//PGMessage()->SendMessage(EPGPlayerMessageType::Spawned, nullptr);
}

void APGCharacterPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	ULocalPlayer* localPlayer = GetController<APlayerController>()->GetLocalPlayer();
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
	
	if (false == CharacterStartUpData.IsNull())
	{
		if (UPGDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.LoadSynchronous())
		{
			LoadedData->GiveToAbilitySystemComponent(AbilitySystemComponent);
		}
	}

	SkillHandler =  FPGHandler::Create<FPGPlayerSkillHandler>();
	SkillHandler->AddSkill(EPGSkillSlot::NormalAttack, 100);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_1, 110);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_2, 111);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_3, 112);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_4, 113);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_5, 114);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_6, 115);

	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_Roll, 10000);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_Jump, 20000);
}

void APGCharacterPlayer::OnHit(UPGStatComponent* InStatComponent)
{
	int32 CurrentHp = PlayerStatComponent->CurrentHealth;

	EPGDamageType DamageType = EPGDamageType::Normal;
	int32 DamageAmount = PlayerStatComponent->CalculateDamage(InStatComponent, DamageType);
	PlayerStatComponent->CurrentHealth = FMath::Max(0, CurrentHp - DamageAmount);

	FPGStatUpdateEventData EventData(EPGStatType::Health,
		PlayerStatComponent->CurrentHealth, PlayerStatComponent->GetStat(EPGStatType::Health));
	PGMessage()->SendMessage(EPGPlayerMessageType::StatUpdate, &EventData);

	if (UPGDamageFloaterManager* Manager = UPGDamageFloaterManager::Get())
	{
		PGDamageFloater()->AddFloater(
			DamageAmount, DamageType, this, true);
	}

	UpdateHpComponent();
	
	if (PlayerStatComponent->CurrentHealth == 0.f)
	{
		UPGAbilityBPLibrary::AddGameplayTagToActorIfNone(this, PGGamePlayTags::Shared_Status_Dead);
	}
}

void APGCharacterPlayer::StartSkillWindow()
{
}

void APGCharacterPlayer::EndSkillWindow()
{

}

void APGCharacterPlayer::SetIsJump(bool IsJump)
{
	bIsJump = IsJump;
}

bool APGCharacterPlayer::IsCanJump() const
{
	if (false == GetIsCacControl())
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
		PGMessage()->SendMessage(EPGPlayerMessageType::Spawned, nullptr);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Meshes loading... %d/%d"), LoadedCount, TotalCount);
	}
}

void APGCharacterPlayer::Input_Move(const FInputActionValue& InputActionValue)
{
	if (false == GetIsCacControl())
	{
		return;
	}
	
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();

	if (MovementVector.SizeSquared() > 0.1f)
	{
		// 카메라(컨트롤러) 방향 기준으로 이동
		const FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
        
		const FVector ForwardDirection = YawRotation.RotateVector(FVector::ForwardVector);
		const FVector RightDirection = YawRotation.RotateVector(FVector::RightVector);
		const FVector MovementDirection = (ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X).GetSafeNormal();
        
		AddMovementInput(MovementDirection, 1.0f);
        
		// 이동 방향으로 캐릭터 회전 (카메라는 독립적으로 공전)
		const FRotator TargetRotation = MovementDirection.Rotation();
		const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, GetWorld()->GetDeltaSeconds(), 8.0f);
		SetActorRotation(NewRotation);
	}
}

void APGCharacterPlayer::Input_Look(const FInputActionValue& InputActionValue)
{
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
	const float Delta = InputActionValue.Get<float>();

	float NewLength = FMath::Clamp(CameraBoom->TargetArmLength - Delta * CameraUpdateSpeed, CameraMinOffset, CameraMaxOffset);
	CameraBoom->TargetArmLength = NewLength;
}

void APGCharacterPlayer::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
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
	PlayerHpWidget->SetHpPercent(static_cast<float>(PlayerStatComponent->CurrentHealth) / PlayerStatComponent->GetStat(EPGStatType::Health));
}

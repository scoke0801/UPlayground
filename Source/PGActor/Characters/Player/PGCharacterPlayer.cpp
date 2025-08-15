// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterPlayer.h"

#include "EnhancedInputSubsystems.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Components/Combat/PGPlayerCombatComponent.h"
#include "PGActor/Components/Input/PGInputComponent.h"
#include "PGActor/Handler/Skill/PGPlayerSkillHandler.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/DataAsset/StartUpData/PGDataAsset_StartUpDataBase.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"

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
}

void APGCharacterPlayer::BeginPlay()
{
	Super::BeginPlay();

	// [TODO] 데이터를 주입할 수 있는 다른 방안을 모색해보자.
	SkillHandler =  FPGHandler::Create<FPGPlayerSkillHandler>();
	SkillHandler->AddSkill(EPGSkillSlot::NormalAttack, 1);
	SkillHandler->AddSkill(EPGSkillSlot::SkillSlot_Dash, 100);
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
	PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Jump,
		ETriggerEvent::Triggered, this, &ThisClass::Input_Jump);
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
}

void APGCharacterPlayer::StartSkillWindow()
{
}

void APGCharacterPlayer::EndSkillWindow()
{

}

void APGCharacterPlayer::Input_Move(const FInputActionValue& InputActionValue)
{
	if (false == GetIsCacControl())
	{
		return;
	}
	
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();

	if (MovementVector.SizeSquared() > 0.1f && false == bIsJump)
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

void APGCharacterPlayer::Input_Jump(const FInputActionValue& InputActionValue)
{
	if (false == GetIsCacControl())
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		if (false == MovementComponent->IsFalling() && false == bIsJump)
		{
			Jump();
			bIsJump = true;
		}
	}
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

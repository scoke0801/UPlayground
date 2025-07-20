// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/LocalPlayer/PGLocalPlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/Input/PGInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Managers/PGDataTableManager.h"
#include "Skill/PGSkillDataRow.h"
#include "Tag/PGGamePlayTags.h"

APGLocalPlayerCharacter::APGLocalPlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	//bUseControllerRotationPitch = false;
	//bUseControllerRotationYaw = false;
	//bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->SocketOffset = FVector(0.f,55.f, 55.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	//GetCharacterMovement()->MaxWalkSpeed = 600.f;
	//GetCharacterMovement()->BrakingDecelerationWalking = 100.f;

	CurrentComboState = EComboState::None;
	CurrentComboCount = 0;
	MaxComboCount = 4;
	bHasQueuedInput = false;
}

void APGLocalPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UPGDataTableManager* dataTableManager = GetGameInstance()->GetSubsystem<UPGDataTableManager>())
	{
		FPGSkillDataRow* skillData = dataTableManager->GetRowData<FPGSkillDataRow>(1);

		if (skillData)
		{
			UE_LOG(LogTemp, Log, TEXT("%d"), skillData->SkillID);
		}
	}
}

void APGLocalPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
	PgInputComponent->BindNativeInputAction(InputConfigDataAsset, PGGamePlayTags::InputTag_Attack,
		ETriggerEvent::Triggered, this, &ThisClass::Input_Attack);
}

void APGLocalPlayerCharacter::StartSkillWindow()
{
	UE_LOG(LogTemp, Log, TEXT("Combo Window Started - Current State: %d, Count: %d"), 
		(int32)CurrentComboState, CurrentComboCount);
	
	SetComboState(EComboState::ComboWindow);
}

void APGLocalPlayerCharacter::EndSkillWindow()
{
	UE_LOG(LogTemp, Log, TEXT("Combo Window Ended - Current State: %d, Count: %d, HasQueuedInput: %s"), 
		(int32)CurrentComboState, CurrentComboCount, bHasQueuedInput ? TEXT("True") : TEXT("False"));
	
	if (bHasQueuedInput)
	{
		bHasQueuedInput = false;
		
		if (CurrentComboCount < MaxComboCount)
		{
			ExecuteAttack(CurrentComboCount + 1);
		}
		else
		{
			ResetCombo();
		}
	}
	else
	{
		SetComboState(EComboState::ComboEnd);
		ResetCombo();
	}
}

void APGLocalPlayerCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();
	const FRotator MovementRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);

	if (0 != MovementVector.Y)
	{
		//const FVector FowardVector = Controller->GetControlRotation().RotateVector(FVector::ForwardVector);
		const FVector FowardVector = MovementRotation.RotateVector(FVector::ForwardVector);
		AddMovementInput(FowardVector, MovementVector.Y);
	}

	if (0 != MovementVector.X)
	{
		//const FVector RightVector = Controller->GetControlRotation().RotateVector(FVector::RightVector);
		const FVector RightVector = MovementRotation.RotateVector(FVector::RightVector);
		AddMovementInput(RightVector, MovementVector.X);
	}
}

void APGLocalPlayerCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
	
	const FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();

	if (0.f != LookAxisVector.X)
	{
		AddControllerYawInput(LookAxisVector.X);
	}

	if (0.f != LookAxisVector.Y)
	{
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APGLocalPlayerCharacter::Input_Attack(const FInputActionValue& InputActionValue)
{
	switch (CurrentComboState)
	{
		case EComboState::None:
		case EComboState::ComboEnd:
		{
			ExecuteAttack(1);
			break;
		}
		case EComboState::Attacking:
		{
			if (CanCombo())
			{
				bHasQueuedInput = true;
				UE_LOG(LogTemp, Log, TEXT("Attack input queued"));
			}
			break;
		}
		case EComboState::ComboWindow:
		{
			if (CanCombo())
			{
				bHasQueuedInput = true;
				UE_LOG(LogTemp, Log, TEXT("Attack input queued during combo window"));
			}
			break;
		}
	}
}

void APGLocalPlayerCharacter::ExecuteAttack(int32 ComboIndex)
{
	if (UPGDataTableManager* dataTableManager = GetGameInstance()->GetSubsystem<UPGDataTableManager>())
	{
		int32 SkillID = ComboIndex;
		FPGSkillDataRow* skillData = dataTableManager->GetRowData<FPGSkillDataRow>(SkillID);

		if (skillData && !skillData->MontagePath.IsNull())
		{
			UAnimMontage* AttackMontage = Cast<UAnimMontage>(skillData->MontagePath.TryLoad());
			
			if (AttackMontage && GetMesh() && GetMesh()->GetAnimInstance())
			{
				if (GetMesh()->GetAnimInstance()->IsAnyMontagePlaying())
				{
					GetMesh()->GetAnimInstance()->StopAllMontages(0.1f);
				}
				
				GetMesh()->GetAnimInstance()->Montage_Play(AttackMontage);
				
				CurrentComboCount = ComboIndex;
				SetComboState(EComboState::Attacking);
				bHasQueuedInput = false;
			}
		}
	}
}

void APGLocalPlayerCharacter::SetComboState(EComboState NewState)
{
	if (CurrentComboState != NewState)
	{
		EComboState PreviousState = CurrentComboState;
		CurrentComboState = NewState;
		
		UE_LOG(LogTemp, Log, TEXT("Combo State Changed: %d -> %d"), 
			(int32)PreviousState, (int32)NewState);
	}
}

void APGLocalPlayerCharacter::ResetCombo()
{
	UE_LOG(LogTemp, Log, TEXT("Combo Reset"));
	
	CurrentComboCount = 0;
	SetComboState(EComboState::None);
	bHasQueuedInput = false;
}

bool APGLocalPlayerCharacter::CanCombo() const
{
	return CurrentComboCount < MaxComboCount;
}

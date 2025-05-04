#include "PGPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "../Combat/PGCombatComponent.h"
#include "../Abilities/PGAbilityComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PGCharacterMovementComponent.h"
#include "../Input/PGEnhancedInputComponent.h"
#include "../Input/PGInputConfig.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

APGPlayerCharacter::APGPlayerCharacter()
{
    // 카메라 붐 생성
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 300.0f;
    CameraBoom->bUsePawnControlRotation = true;
    
    // 팔로우 카메라 생성
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
    
    // 캐릭터 기본 회전 설정
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;  // 이동 방향으로 캐릭터 회전
    bUseControllerRotationRoll = false;
    
    // 캐릭터 무브먼트 설정
    GetCharacterMovement()->bOrientRotationToMovement = true; // 캐릭터가 이동 방향으로 회전
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // 회전 속도
    GetCharacterMovement()->JumpZVelocity = 600.0f;
    GetCharacterMovement()->AirControl = 0.2f;
}

void APGPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // 로컬 플레이어인 경우에만 입력 매핑 컨텍스트 추가
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }
}

void APGPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    // Cast to enhanced input component
    PGInputComponent = Cast<UPGEnhancedInputComponent>(PlayerInputComponent);
    
    if (PGInputComponent && InputConfig)
    {
        // Movement
        if (InputConfig->MoveAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->MoveAction, ETriggerEvent::Triggered, this, &APGPlayerCharacter::Move);
        }
        
        if (InputConfig->LookAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->LookAction, ETriggerEvent::Triggered, this, &APGPlayerCharacter::Look);
        }
        
        if (InputConfig->JumpAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->JumpAction, ETriggerEvent::Started, this, &APGPlayerCharacter::StartJump);
            PGInputComponent->BindActionSafe(InputConfig->JumpAction, ETriggerEvent::Completed, this, &APGPlayerCharacter::StopJump);
        }
        
        if (InputConfig->SprintAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->SprintAction, ETriggerEvent::Started, this, &APGPlayerCharacter::StartSprint);
            PGInputComponent->BindActionSafe(InputConfig->SprintAction, ETriggerEvent::Completed, this, &APGPlayerCharacter::StopSprint);
        }
        
        // Combat
        if (InputConfig->PrimaryAttackAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->PrimaryAttackAction, ETriggerEvent::Triggered, this, &APGPlayerCharacter::PrimaryAttack);
        }
        
        if (InputConfig->SecondaryAttackAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->SecondaryAttackAction, ETriggerEvent::Triggered, this, &APGPlayerCharacter::SecondaryAttack);
        }
        
        if (InputConfig->TargetAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->TargetAction, ETriggerEvent::Triggered, this, &APGPlayerCharacter::SelectTarget);
        }
        
        if (InputConfig->ClearTargetAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->ClearTargetAction, ETriggerEvent::Triggered, this, &APGPlayerCharacter::ClearTarget);
        }
        
        // Abilities
        if (InputConfig->Ability1Action)
        {
            PGInputComponent->BindActionSafe(InputConfig->Ability1Action, ETriggerEvent::Triggered, this, &APGPlayerCharacter::UseAbility1);
        }
        
        if (InputConfig->Ability2Action)
        {
            PGInputComponent->BindActionSafe(InputConfig->Ability2Action, ETriggerEvent::Triggered, this, &APGPlayerCharacter::UseAbility2);
        }
        
        if (InputConfig->Ability3Action)
        {
            PGInputComponent->BindActionSafe(InputConfig->Ability3Action, ETriggerEvent::Triggered, this, &APGPlayerCharacter::UseAbility3);
        }
        
        if (InputConfig->Ability4Action)
        {
            PGInputComponent->BindActionSafe(InputConfig->Ability4Action, ETriggerEvent::Triggered, this, &APGPlayerCharacter::UseAbility4);
        }
        
        // Interaction
        if (InputConfig->InteractAction)
        {
            PGInputComponent->BindActionSafe(InputConfig->InteractAction, ETriggerEvent::Triggered, this, &APGPlayerCharacter::InteractInput);
        }
    }
}

void APGPlayerCharacter::SetCameraDistance(float Distance)
{
    if (CameraBoom)
    {
        CameraBoom->TargetArmLength = Distance;
    }
}

void APGPlayerCharacter::SelectTarget()
{
    if (CombatComponent)
    {
        CombatComponent->SelectTarget();
    }
}

void APGPlayerCharacter::ClearTarget()
{
    if (CombatComponent)
    {
        CombatComponent->ClearTarget();
    }
}

void APGPlayerCharacter::Interact(APGBaseCharacter* Interactor)
{
    // 플레이어가 상호작용을 받았을 때의 처리
    Super::Interact(Interactor);
    
    // 플레이어 특화된 상호작용 처리
    UE_LOG(LogTemp, Log, TEXT("Player Character Interacted"));
}

void APGPlayerCharacter::InteractInput(const FInputActionValue& Value)
{
    // 플레이어가 상호작용 키를 눌렀을 때 근처 오브젝트와 상호작용
    UE_LOG(LogTemp, Log, TEXT("Player pressed Interact button"));
    
    // 여기서 주변 상호작용 가능한 오브젝트를 탐색하고 상호작용을 시작할 수 있음
    // 예: 가장 가까운 NPC 찾기, 아이템 줍기, 문 열기 등
}

void APGPlayerCharacter::Move(const FInputActionValue& Value)
{
    // Movement input
    const FVector2D MovementVector = Value.Get<FVector2D>();
    
    if (Controller != nullptr)
    {
        // 컨트롤러 회전 가져오기
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        
        // 이동 방향 계산
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        
        // 이동 입력 적용
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void APGPlayerCharacter::Look(const FInputActionValue& Value)
{
    // Look input
    const FVector2D LookAxisVector = Value.Get<FVector2D>();
    
    if (Controller != nullptr)
    {
        // Yaw(좌우) 및 Pitch(상하) 회전 추가
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void APGPlayerCharacter::StartJump(const FInputActionValue& Value)
{
    // 커스텀 이동 컴포넌트 사용
    if (UPGCharacterMovementComponent* PGMovement = Cast<UPGCharacterMovementComponent>(GetCharacterMovement()))
    {
        // 상태 확인 후 점프
        if (PGMovement->GetMovementState() != EPGMovementState::Stunned && 
            PGMovement->GetMovementState() != EPGMovementState::Knocked)
        {
            Jump();
        }
    }
    else
    {
        Jump();
    }
}

void APGPlayerCharacter::StopJump(const FInputActionValue& Value)
{
    StopJumping();
}

void APGPlayerCharacter::StartSprint(const FInputActionValue& Value)
{
    // 커스텀 이동 컴포넌트 사용
    if (UPGCharacterMovementComponent* PGMovement = Cast<UPGCharacterMovementComponent>(GetCharacterMovement()))
    {
        PGMovement->StartSprinting();
    }
}

void APGPlayerCharacter::StopSprint(const FInputActionValue& Value)
{
    // 커스텀 이동 컴포넌트 사용
    if (UPGCharacterMovementComponent* PGMovement = Cast<UPGCharacterMovementComponent>(GetCharacterMovement()))
    {
        PGMovement->StopSprinting();
    }
}

void APGPlayerCharacter::PrimaryAttack(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->PrimaryAttack();
    }
}

void APGPlayerCharacter::SecondaryAttack(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->SecondaryAttack();
    }
}

void APGPlayerCharacter::UseAbility1(const FInputActionValue& Value)
{
    if (AbilityComponent)
    {
        // 실제 구현에서는 특정 능력 ID를 사용
        AbilityComponent->ActivateAbility(FName("Ability1"));
    }
}

void APGPlayerCharacter::UseAbility2(const FInputActionValue& Value)
{
    if (AbilityComponent)
    {
        AbilityComponent->ActivateAbility(FName("Ability2"));
    }
}

void APGPlayerCharacter::UseAbility3(const FInputActionValue& Value)
{
    if (AbilityComponent)
    {
        AbilityComponent->ActivateAbility(FName("Ability3"));
    }
}

void APGPlayerCharacter::UseAbility4(const FInputActionValue& Value)
{
    if (AbilityComponent)
    {
        AbilityComponent->ActivateAbility(FName("Ability4"));
    }
}

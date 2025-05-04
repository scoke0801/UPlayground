#include "PGCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"

UPGCharacterMovementComponent::UPGCharacterMovementComponent()
{
    // 기본 설정
    CurrentMovementState = EPGMovementState::Walking;
    
    // 회피 설정
    DodgeStrength = 1000.0f;
    DodgeDuration = 0.3f;
    DodgeCooldown = 1.0f;
    bIsDodging = false;
    bDodgeOnCooldown = false;
    
    // 달리기 설정
    SprintSpeedMultiplier = 1.5f;
    DefaultWalkSpeed = 600.0f;
    MaxWalkSpeed = DefaultWalkSpeed;
}

void UPGCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UPGCharacterMovementComponent, CurrentMovementState);
}

EPGMovementState UPGCharacterMovementComponent::GetMovementState() const
{
    return CurrentMovementState;
}

void UPGCharacterMovementComponent::SetMovementState(EPGMovementState NewState)
{
    if (CurrentMovementState != NewState)
    {
        CurrentMovementState = NewState;
        
        // 상태에 따른 처리
        switch (NewState)
        {
            case EPGMovementState::Walking:
                MaxWalkSpeed = DefaultWalkSpeed;
                break;
                
            case EPGMovementState::Sprinting:
                MaxWalkSpeed = DefaultWalkSpeed * SprintSpeedMultiplier;
                break;
                
            case EPGMovementState::Dodging:
                // 회피 중에는 이동 제어 제한
                break;
                
            case EPGMovementState::Stunned:
                // 스턴 상태에서는 이동 불가
                StopMovementImmediately();
                DisableMovement();
                break;
                
            case EPGMovementState::Knocked:
                // 넉백 상태
                StopMovementImmediately();
                break;
        }
    }
}

bool UPGCharacterMovementComponent::CanDodge() const
{
    return !bIsDodging && !bDodgeOnCooldown && 
           CurrentMovementState != EPGMovementState::Stunned && 
           CurrentMovementState != EPGMovementState::Knocked;
}

void UPGCharacterMovementComponent::Dodge(FVector Direction)
{
    if (!CanDodge())
    {
        return;
    }
    
    // 회피 시작
    bIsDodging = true;
    DodgeDirection = Direction.GetSafeNormal();
    
    // 회피 상태로 변경
    SetMovementState(EPGMovementState::Dodging);
    
    // 회피 타이머 설정
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(DodgeTimerHandle, [this]()
        {
            bIsDodging = false;
            SetMovementState(EPGMovementState::Walking);
            
            // 쿨다운 시작
            bDodgeOnCooldown = true;
            GetWorld()->GetTimerManager().SetTimer(DodgeCooldownTimerHandle, [this]()
            {
                bDodgeOnCooldown = false;
            }, DodgeCooldown, false);
            
        }, DodgeDuration, false);
    }
}

void UPGCharacterMovementComponent::StartSprinting()
{
    if (CurrentMovementState == EPGMovementState::Walking)
    {
        SetMovementState(EPGMovementState::Sprinting);
    }
}

void UPGCharacterMovementComponent::StopSprinting()
{
    if (CurrentMovementState == EPGMovementState::Sprinting)
    {
        SetMovementState(EPGMovementState::Walking);
    }
}

void UPGCharacterMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
    // 회피 중일 때 특별한 물리 처리
    if (CurrentMovementState == EPGMovementState::Dodging && bIsDodging)
    {
        PerformDodge(deltaTime);
    }
    else
    {
        Super::PhysWalking(deltaTime, Iterations);
    }
}

void UPGCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    Super::PhysCustom(deltaTime, Iterations);
}

void UPGCharacterMovementComponent::PerformDodge(float DeltaTime)
{
    if (CharacterOwner)
    {
        // 회피 이동 적용
        FVector DodgeVelocity = DodgeDirection * DodgeStrength;
        Velocity = DodgeVelocity;
        
        // 회피 중 중력 적용
        const FVector Gravity(0.0f, 0.0f, GetGravityZ());
        Velocity += Gravity * DeltaTime;
        
        // 이동 적용
        FHitResult Hit;
        SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit);
        
        if (Hit.bBlockingHit)
        {
            // 충돌 시 슬라이딩 처리
            SlideAlongSurface(Velocity, 1.0f - Hit.Time, Hit.Normal, Hit, true);
        }
    }
}

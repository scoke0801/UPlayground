// Copyright (c) 2025. All Rights Reserved.

#include "PGCharacterManager.h"
#include "../../Characters/PGBaseCharacter.h"
#include "../../Characters/PGPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UPGCharacterManager::UPGCharacterManager()
{
    ManagerName = FName("CharacterManager");
}

bool UPGCharacterManager::OnInitialize_Implementation()
{
    if (!Super::OnInitialize_Implementation())
    {
        return false;
    }

    return true;
}

void UPGCharacterManager::OnShutdown_Implementation()
{
    // 모든 캐릭터 정리
    for (APGBaseCharacter* Character : ManagedCharacters)
    {
        if (IsValid(Character))
        {
            Character->Destroy();
        }
    }
    
    ManagedCharacters.Empty();
    PlayerCharacter = nullptr;

    Super::OnShutdown_Implementation();
}

void UPGCharacterManager::OnUpdate_Implementation(float DeltaTime)
{
    Super::OnUpdate_Implementation(DeltaTime);

    // 필요한 경우 캐릭터 업데이트 로직 추가
}

void UPGCharacterManager::OnGameStateChange_Implementation(EPGGameStateType NewState)
{
    Super::OnGameStateChange_Implementation(NewState);

    // 게임 상태에 따른 캐릭터 관리 로직
    switch (NewState)
    {
        case EPGGameStateType::GamePaused:
            // 캐릭터 일시정지 처리
            break;
            
        case EPGGameStateType::GameRunning:
            // 캐릭터 활성화 처리
            break;
            
        default:
            break;
    }
}

APGBaseCharacter* UPGCharacterManager::SpawnCharacter(TSubclassOf<APGBaseCharacter> CharacterClass, const FTransform& SpawnTransform, AActor* Owner)
{
    if (!CharacterClass || !GetWorld())
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APGBaseCharacter* NewCharacter = GetWorld()->SpawnActor<APGBaseCharacter>(CharacterClass, SpawnTransform, SpawnParams);
    
    if (NewCharacter)
    {
        RegisterCharacter(NewCharacter);
        InitializeCharacter(NewCharacter);
        OnCharacterSpawned.Broadcast(NewCharacter);
    }

    return NewCharacter;
}

bool UPGCharacterManager::DestroyCharacter(APGBaseCharacter* Character)
{
    if (!IsValid(Character))
    {
        return false;
    }

    UnregisterCharacter(Character);
    Character->Destroy();
    OnCharacterDestroyed.Broadcast(Character);

    return true;
}

APGBaseCharacter* UPGCharacterManager::FindCharacterByID(FName CharacterID) const
{
    // 기존 시스템에서는 ID 시스템이 없으므로 이 기능은 추후 구현
    return nullptr;
}

TArray<APGBaseCharacter*> UPGCharacterManager::GetAllCharacters() const
{
    TArray<APGBaseCharacter*> Result;
    
    for (APGBaseCharacter* Character : ManagedCharacters)
    {
        if (IsValid(Character))
        {
            Result.Add(Character);
        }
    }
    
    return Result;
}

APGBaseCharacter* UPGCharacterManager::GetPlayerCharacter() const
{
    return PlayerCharacter;
}

APGBaseCharacter* UPGCharacterManager::GetClosestCharacterToPlayer(float MaxDistance) const
{
    if (!PlayerCharacter)
    {
        return nullptr;
    }

    return GetClosestCharacterToLocation(PlayerCharacter->GetActorLocation(), MaxDistance);
}

APGBaseCharacter* UPGCharacterManager::GetClosestCharacterToLocation(const FVector& Location, float MaxDistance) const
{
    APGBaseCharacter* ClosestCharacter = nullptr;
    float ClosestDistance = MaxDistance > 0.0f ? MaxDistance : MAX_FLT;

    for (APGBaseCharacter* Character : ManagedCharacters)
    {
        if (!IsValid(Character) || Character->GetCharacterState() == EPGCharacterState::Dead)
        {
            continue;
        }

        float Distance = FVector::Dist(Location, Character->GetActorLocation());
        if (Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestCharacter = Character;
        }
    }

    return ClosestCharacter;
}

int32 UPGCharacterManager::GetAliveCharacterCount() const
{
    int32 Count = 0;
    
    for (APGBaseCharacter* Character : ManagedCharacters)
    {
        if (IsValid(Character) && Character->GetCharacterState() != EPGCharacterState::Dead)
        {
            Count++;
        }
    }
    
    return Count;
}

void UPGCharacterManager::RegisterCharacter(APGBaseCharacter* Character)
{
    if (!IsValid(Character))
    {
        return;
    }

    ManagedCharacters.AddUnique(Character);
    
    // 플레이어 캐릭터 확인
    if (Character->IsA<APGPlayerCharacter>())
    {
        PlayerCharacter = Character;
    }
}

void UPGCharacterManager::UnregisterCharacter(APGBaseCharacter* Character)
{
    if (!IsValid(Character))
    {
        return;
    }

    ManagedCharacters.Remove(Character);
    
    if (PlayerCharacter == Character)
    {
        PlayerCharacter = nullptr;
    }
}

void UPGCharacterManager::InitializeCharacter(APGBaseCharacter* Character)
{
    if (!IsValid(Character))
    {
        return;
    }

    // 기존 시스템에서는 별도의 초기화가 필요하지 않음
}

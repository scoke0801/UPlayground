// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../PGBaseManager.h"
#include "../../Characters/PGBaseCharacter.h"
#include "PGCharacterManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterSpawned, APGBaseCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDestroyed, APGBaseCharacter*, Character);

/**
 * 캐릭터 매니저
 * 게임 내 모든 캐릭터의 생성, 소멸, 관리를 담당합니다.
 */
UCLASS(Blueprintable)
class UPLAYGROUND_API UPGCharacterManager : public UPGBaseManager
{
    GENERATED_BODY()

public:
    UPGCharacterManager();

    //~Begin UPGBaseManager interface
    virtual bool OnInitialize_Implementation() override;
    virtual void OnShutdown_Implementation() override;
    virtual void OnUpdate_Implementation(float DeltaTime) override;
    virtual void OnGameStateChange_Implementation(EPGGameStateType NewState) override;
    //~End UPGBaseManager interface

    /** 캐릭터 생성 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    APGBaseCharacter* SpawnCharacter(TSubclassOf<APGBaseCharacter> CharacterClass, const FTransform& SpawnTransform, AActor* Owner = nullptr);

    /** 캐릭터 제거 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    bool DestroyCharacter(APGBaseCharacter* Character);

    /** ID로 캐릭터 찾기 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    APGBaseCharacter* FindCharacterByID(FName CharacterID) const;

    /** 모든 캐릭터 목록 조회 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    TArray<APGBaseCharacter*> GetAllCharacters() const;

    /** 플레이어 캐릭터 반환 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    APGBaseCharacter* GetPlayerCharacter() const;

    /** 플레이어 위치 기준 가장 가까운 캐릭터 찾기 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    APGBaseCharacter* GetClosestCharacterToPlayer(float MaxDistance = 0.0f) const;

    /** 특정 위치에서 가장 가까운 캐릭터 찾기 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    APGBaseCharacter* GetClosestCharacterToLocation(const FVector& Location, float MaxDistance = 0.0f) const;

    /** 살아있는 캐릭터 수 반환 */
    UFUNCTION(BlueprintCallable, Category = "Character")
    int32 GetAliveCharacterCount() const;

    // 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Character|Events")
    FOnCharacterSpawned OnCharacterSpawned;

    UPROPERTY(BlueprintAssignable, Category = "Character|Events")
    FOnCharacterDestroyed OnCharacterDestroyed;

protected:
    /** 관리 중인 모든 캐릭터 */
    UPROPERTY()
    TArray<APGBaseCharacter*> ManagedCharacters;

    /** 플레이어 캐릭터 참조 */
    UPROPERTY()
    APGBaseCharacter* PlayerCharacter;

    // 캐릭터 등록/해제
    void RegisterCharacter(APGBaseCharacter* Character);
    void UnregisterCharacter(APGBaseCharacter* Character);

    // 캐릭터 초기화
    void InitializeCharacter(APGBaseCharacter* Character);
};

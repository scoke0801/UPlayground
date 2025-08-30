// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGProjectileManager.generated.h"

/**
 * 
 */
UCLASS()
class PGACTOR_API UPGProjectileManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	static TWeakObjectPtr<UPGProjectileManager> WeakThis;

private:
	// 풀 맵
	UPROPERTY()
	TMap<EPGProjectileType, class APGProjectilePool*> ProjectilePools;

public:
	static UPGProjectileManager* Get();
    
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	
	// 투사체 발사 (메인 인터페이스)
	UFUNCTION(BlueprintCallable, Category = "Projectile Manager")
	class APGPooledProjectile* FireProjectile(int32 ProjectileId, 
										   const FVector& Origin, 
										   const FVector& Direction,
										   float Speed = -1.0f);

	// 풀 관리
	UFUNCTION(BlueprintCallable, Category = "Pool Management")
	void InitializeProjectilePool(EPGProjectileType Type);
	
private:
	void PreAllocPools();

	class APGProjectilePool* CreateProjectilePool(EPGProjectileType Type);
};

#define PGProjectile() UPGProjectileManager::Get()
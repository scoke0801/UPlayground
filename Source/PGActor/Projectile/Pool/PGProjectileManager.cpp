// Fill out your copyright notice in the Description page of Project Settings.

#include "PGProjectileManager.h"
#include "PGPooledProjectile.h"
#include "PGProjectilePool.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Projectile/PGProjectileDataRow.h"
#include "PGData/DataTable/Projectile/PGProjectilePoolDataRow.h"
#include "Engine/World.h"

TWeakObjectPtr<UPGProjectileManager> UPGProjectileManager::WeakThis = nullptr;

UPGProjectileManager* UPGProjectileManager::Get()
{
	if (WeakThis.IsValid())
	{
		return WeakThis.Get();
	}

	return nullptr;
}

void UPGProjectileManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PreAllocPools();
	
	WeakThis = MakeWeakObjectPtr(this);
	
	UE_LOG(LogTemp, Log, TEXT("ProjectileManager Initialized"));
}

void UPGProjectileManager::Deinitialize()
{
	// 모든 풀 정리
	CleanupAllPools();
	
	WeakThis = nullptr;
	
	Super::Deinitialize();
	
	UE_LOG(LogTemp, Log, TEXT("ProjectileManager Deinitialized"));
}

APGPooledProjectile* UPGProjectileManager::FireProjectile(int32 ProjectileId, const FGenericTeamId OwnerTeamId,  const FVector& Origin,
	const FVector& Direction, float Speed)
{
	UPGDataTableManager* DataTableManager = UPGDataTableManager::Get();
	if (nullptr == DataTableManager) { return nullptr; }
	
	const FPGProjectileDataRow* ProjectileData = DataTableManager->GetRowData<FPGProjectileDataRow>(ProjectileId);
	if (nullptr == ProjectileData) { return nullptr; }
	
	const FPGProjectilePoolDataRow* Setting = DataTableManager->GetRowData<FPGProjectilePoolDataRow>(
		StaticCast<int32>(ProjectileData->ProjectileType));
	if (nullptr == Setting) { return nullptr; }

	// 풀이 없으면 생성
	if (!ProjectilePools.Contains(ProjectileData->ProjectileType))
	{
		InitializeProjectilePool(ProjectileData->ProjectileType);
	}
	
	UPGProjectilePool* Pool = ProjectilePools.FindRef(ProjectileData->ProjectileType);
	if (!Pool) { return nullptr; }

	// 투사체 요청
	APGPooledProjectile* Projectile = Pool->RequestProjectile();
	if (!Projectile) { return nullptr; }

	// 설정 적용
	float FinalSpeed = (Speed > 0) ? Speed : (ProjectileData ? ProjectileData->DefaultSpeed : 1000.0f);
	float FinalDamage = ProjectileData ? ProjectileData->DefaultDamage : 10.0f;

	// 발사
	Projectile->Fire(OwnerTeamId, Origin, Direction, FinalSpeed, FinalDamage);
	
	return Projectile;
}

void UPGProjectileManager::InitializeProjectilePool(EPGProjectileType Type)
{
	if (ProjectilePools.Contains(Type)) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Pool already exists for type: %s"), 
			*UEnum::GetValueAsString(Type));
		return;
	}

	UPGProjectilePool* NewPool = CreateProjectilePool(Type);
	if (NewPool)
	{
		ProjectilePools.Add(Type, NewPool);
		UE_LOG(LogTemp, Log, TEXT("Created pool for type: %s"), 
			*UEnum::GetValueAsString(Type));
	}
}

void UPGProjectileManager::PreAllocPools()
{
	InitializeProjectilePool(EPGProjectileType::Arrow);
}

UPGProjectilePool* UPGProjectileManager::CreateProjectilePool(EPGProjectileType Type)
{
	UPGDataTableManager* DataTableManager = UPGDataTableManager::Get();
	if (nullptr == DataTableManager)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateProjectilePool - DataTableManager is null"));
		return nullptr;
	}
	
	const FPGProjectilePoolDataRow* Setting = DataTableManager->GetRowData<FPGProjectilePoolDataRow>(
		StaticCast<int32>(Type));
	if (nullptr == Setting || false == Setting->ProjectileClassPath.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateProjectilePool - Invalid pool settings for type: %s"), 
			*UEnum::GetValueAsString(Type));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World) 
	{
		UE_LOG(LogTemp, Error, TEXT("CreateProjectilePool - World is null"));
		return nullptr;
	}

	// UObject 기반 Pool 생성
	FString PoolName = FString::Printf(TEXT("ProjectilePool_%s"), 
		*UEnum::GetValueAsString(Type));
	
	UPGProjectilePool* NewPool = NewObject<UPGProjectilePool>(
		this,  // Outer를 Manager로 설정 (GC 관리)
		UPGProjectilePool::StaticClass(),
		FName(*PoolName),
		RF_NoFlags  // 일반 플래그 사용
	);
	
	if (NewPool)
	{
		// Pool 초기화
		NewPool->Initialize(World);
		NewPool->SetProjectileClass(Setting->ProjectileClassPath);
		NewPool->SetPoolSize(Setting->PoolSize, Setting->MaxPoolSize);
		NewPool->InitializePool();
		
		UE_LOG(LogTemp, Log, TEXT("Created ProjectilePool: %s, Initial: %d, Max: %d"), 
			*PoolName, Setting->PoolSize, Setting->MaxPoolSize);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create ProjectilePool for type: %s"), 
			*UEnum::GetValueAsString(Type));
	}

	return NewPool;
}

void UPGProjectileManager::CleanupAllPools()
{
	for (auto& Pair : ProjectilePools)
	{
		if (UPGProjectilePool* Pool = Pair.Value)
		{
			Pool->CleanupPool();
		}
	}
	ProjectilePools.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("All projectile pools cleaned up"));
}
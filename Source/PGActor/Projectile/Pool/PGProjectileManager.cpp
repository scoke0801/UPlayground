// Fill out your copyright notice in the Description page of Project Settings.


#include "PGProjectileManager.h"

#include "PGPooledProjectile.h"
#include "PGProjectilePool.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Projectile/PGProjectileDataRow.h"
#include "PGData/DataTable/Projectile/PGProjectilePoolDataRow.h"

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
}

void UPGProjectileManager::Deinitialize()
{
    WeakThis = nullptr;
	
	Super::Deinitialize();
}

APGPooledProjectile* UPGProjectileManager::FireProjectile(int32 ProjectileId, const FVector& Origin,
	const FVector& Direction, float Speed)
{
	UPGDataTableManager* DataTableManager = UPGDataTableManager::Get();
	if (nullptr == DataTableManager) { return nullptr; }
	
	const FPGProjectileDataRow* ProjectileData = DataTableManager->GetRowData<FPGProjectileDataRow>(ProjectileId);
	if (nullptr == ProjectileData) { return nullptr; }
	
	const FPGProjectilePoolDataRow* Setting = DataTableManager->GetRowData<FPGProjectilePoolDataRow>(
		StaticCast<int32>(ProjectileData->ProjectileType));
	if (nullptr == Setting) {return nullptr;}

	// 풀이 없으면 생성
	if (!ProjectilePools.Contains(ProjectileData->ProjectileType))
	{
		InitializeProjectilePool(ProjectileData->ProjectileType);
	}
	
	APGProjectilePool* Pool = ProjectilePools.FindRef(ProjectileData->ProjectileType);
	if (!Pool) { return nullptr; }

	// 투사체 요청
	APGPooledProjectile* Projectile = Pool->RequestProjectile();
	if (!Projectile) { return nullptr; }

	// 설정 적용
	float FinalSpeed = (Speed > 0) ? Speed : (ProjectileData ? ProjectileData->DefaultSpeed : 1000.0f);
	float FinalDamage = ProjectileData ? ProjectileData->DefaultDamage : 10.0f;

	// 발사
	Projectile->Fire(Origin, Direction, FinalSpeed, FinalDamage);
    
	return Projectile;
}

void UPGProjectileManager::InitializeProjectilePool(EPGProjectileType Type)
{
	if (ProjectilePools.Contains(Type)) return;

	APGProjectilePool* NewPool = CreateProjectilePool(Type);
	if (NewPool)
	{
		ProjectilePools.Add(Type, NewPool);
	}
}

void UPGProjectileManager::PreAllocPools()
{
	InitializeProjectilePool(EPGProjectileType::Arrow);
}

APGProjectilePool* UPGProjectileManager::CreateProjectilePool(EPGProjectileType Type)
{
	UPGDataTableManager* DataTableManager = UPGDataTableManager::Get();
	if (nullptr == DataTableManager)
	{
		return nullptr;
	}
	
	const FPGProjectilePoolDataRow* Setting = DataTableManager->GetRowData<FPGProjectilePoolDataRow>(
		StaticCast<int32>(Type));
	if (nullptr == Setting || false == Setting->ProjectileClassPath.IsValid())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*FString::Printf(TEXT("ProjectilePool_%s"), 
							*UEnum::GetValueAsString(Type)));
    
	APGProjectilePool* NewPool = World->SpawnActor<APGProjectilePool>(SpawnParams);
	if (NewPool)
	{
		NewPool->SetProjectileClass(Setting->ProjectileClassPath);
		NewPool->SetPoolSize(Setting->PoolSize, Setting->MaxPoolSize);
		NewPool->InitializePool();
	}

	return NewPool;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "PGProjectilePool.h"

#include "PGPooledProjectile.h"

// Sets default values
APGProjectilePool::APGProjectilePool()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

APGPooledProjectile* APGProjectilePool::RequestProjectile()
{
	APGPooledProjectile* Projectile = nullptr;

	if (AvailableProjectiles.Num() > 0)
	{
		Projectile = AvailableProjectiles.Pop();
	}
	else if (ActiveProjectiles.Num() < MaxPoolSize)
	{
		Projectile = CreateNewProjectile();
	}

	if (Projectile)
	{
		ActiveProjectiles.Add(Projectile);
		Projectile->OwnerPool = this;
	}

	return Projectile;
}

void APGProjectilePool::ReturnProjectile(APGPooledProjectile* Projectile)
{
	if (!Projectile) return;

	if (ActiveProjectiles.Remove(Projectile) > 0)
	{
		AvailableProjectiles.Add(Projectile);
		Projectile->OwnerPool = nullptr;
	}
}

void APGProjectilePool::SetProjectileClass(const FSoftClassPath& InClassPath)
{
	ProjectileClassPath = InClassPath;
	ProjectileClass = nullptr; // 캐시 초기화
    
	// 즉시 로드 시도
	LoadProjectileClass();
}

void APGProjectilePool::InitializePool()
{
	for (int32 i = 0; i < InitialPoolSize; ++i)
	{
		APGPooledProjectile* NewProjectile = CreateNewProjectile();
		if (NewProjectile)
		{
			AvailableProjectiles.Add(NewProjectile);
		}
	}
}

APGPooledProjectile* APGProjectilePool::CreateNewProjectile()
{
	// 클래스가 로드되지 않았으면 로드 시도
	if (!LoadProjectileClass()) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
    
	APGPooledProjectile* NewProjectile = GetWorld()->SpawnActor<APGPooledProjectile>(
		ProjectileClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (NewProjectile)
	{
		NewProjectile->SetActorHiddenInGame(true);
		NewProjectile->SetActorEnableCollision(false);
	}

	return NewProjectile;
}

bool APGProjectilePool::IsProjectileClassLoaded() const
{
	return ProjectileClass != nullptr;
}

bool APGProjectilePool::LoadProjectileClass()
{
	if (ProjectileClass)
	{
		// 이미 로드됨
		return true; 
	}
	
	if (!ProjectileClassPath.IsValid()) 
	{
		return false;
	}

	// 소프트 클래스 로드
	if (UClass* LoadedClass = ProjectileClassPath.TryLoadClass<APGPooledProjectile>())
	{
		ProjectileClass = LoadedClass;
		return true;
	}
    
	return false;
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "PGProjectilePool.h"
#include "PGPooledProjectile.h"
#include "Engine/World.h"

UWorld* UPGProjectilePool::GetWorld() const
{
	return CachedWorld;
}

void UPGProjectilePool::BeginDestroy()
{
	// 풀 정리
	CleanupPool();
	
	Super::BeginDestroy();
}

void UPGProjectilePool::Initialize(UWorld* InWorld)
{
	CachedWorld = InWorld;
	
	// 디버그 로그
	UE_LOG(LogTemp, Log, TEXT("ProjectilePool Initialize - World: %s"), 
		CachedWorld ? *CachedWorld->GetName() : TEXT("NULL"));
}

APGPooledProjectile* UPGProjectilePool::RequestProjectile()
{
	APGPooledProjectile* Projectile = nullptr;

	// 사용 가능한 투사체가 있으면 재사용
	if (AvailableProjectiles.Num() > 0)
	{
		Projectile = AvailableProjectiles.Pop();
		
		// 유효성 체크
		if (!IsValid(Projectile))
		{
			Projectile = nullptr;
		}
	}
	
	// 새로운 투사체 생성
	if (!Projectile && ActiveProjectiles.Num() < MaxPoolSize)
	{
		Projectile = CreateNewProjectile();
	}

	// 활성 목록에 추가
	if (Projectile)
	{
		ActiveProjectiles.Add(Projectile);
	}

	return Projectile;
}

void UPGProjectilePool::ReturnProjectile(APGPooledProjectile* Projectile)
{
	if (!IsValid(Projectile)) return;

	// 활성 목록에서 제거하고 사용 가능 목록으로 이동
	if (ActiveProjectiles.Remove(Projectile) > 0)
	{
		AvailableProjectiles.Add(Projectile);
	}
}

void UPGProjectilePool::SetProjectileClass(const FSoftClassPath& InClassPath)
{
	ProjectileClassPath = InClassPath;
	ProjectileClass = nullptr; // 캐시 초기화
	
	// 즉시 로드 시도
	LoadProjectileClass();
}

void UPGProjectilePool::InitializePool()
{
	if (!CachedWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("ProjectilePool::InitializePool - No World cached!"));
		return;
	}
	
	// 미리 생성할 투사체 생성
	for (int32 i = 0; i < InitialPoolSize; ++i)
	{
		APGPooledProjectile* NewProjectile = CreateNewProjectile();
		if (NewProjectile)
		{
			AvailableProjectiles.Add(NewProjectile);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("ProjectilePool Initialized - Created %d projectiles"), 
		AvailableProjectiles.Num());
}

void UPGProjectilePool::CleanupPool()
{
	// 모든 투사체 제거
	auto DestroyProjectiles = [](TArray<APGPooledProjectile*>& Projectiles)
	{
		for (APGPooledProjectile* Projectile : Projectiles)
		{
			if (IsValid(Projectile))
			{
				Projectile->Destroy();
			}
		}
		Projectiles.Empty();
	};
	
	DestroyProjectiles(AvailableProjectiles);
	DestroyProjectiles(ActiveProjectiles);
	
	CachedWorld = nullptr;
}

APGPooledProjectile* UPGProjectilePool::CreateNewProjectile()
{
	// World 체크
	if (!CachedWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNewProjectile - No World cached!"));
		return nullptr;
	}
	
	// 클래스가 로드되지 않았으면 로드 시도
	if (!LoadProjectileClass()) 
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNewProjectile - Failed to load projectile class"));
		return nullptr;
	}

	// 투사체 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = 
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	APGPooledProjectile* NewProjectile = CachedWorld->SpawnActor<APGPooledProjectile>(
		ProjectileClass, 
		FVector::ZeroVector, 
		FRotator::ZeroRotator, 
		SpawnParams
	);

	if (NewProjectile)
	{
		// 초기 상태 설정
		NewProjectile->SetActorHiddenInGame(true);
		NewProjectile->SetActorEnableCollision(false);
		
		// Pool 참조 설정 (한 번만 설정하고 변경하지 않음)
		NewProjectile->OwnerPool = this;
		
		// 수명 무한으로 설정 (자동 제거 방지)
		NewProjectile->SetLifeSpan(0.0f);
	}

	return NewProjectile;
}

bool UPGProjectilePool::IsProjectileClassLoaded() const
{
	return ProjectileClass != nullptr;
}

bool UPGProjectilePool::LoadProjectileClass()
{
	if (ProjectileClass)
	{
		// 이미 로드됨
		return true;
	}
	
	if (!ProjectileClassPath.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LoadProjectileClass - Invalid class path"));
		return false;
	}

	// 소프트 클래스 로드
	if (UClass* LoadedClass = ProjectileClassPath.TryLoadClass<APGPooledProjectile>())
	{
		ProjectileClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("LoadProjectileClass - Successfully loaded: %s"), 
			*LoadedClass->GetName());
		return true;
	}
	
	UE_LOG(LogTemp, Error, TEXT("LoadProjectileClass - Failed to load class"));
	return false;
}
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PGProjectilePool.generated.h"

class APGPooledProjectile;

/**
 * UObject 기반 투사체 풀
 */
UCLASS()
class PGACTOR_API UPGProjectilePool : public UObject
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Pool Settings")
	FSoftClassPath ProjectileClassPath;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Pool Settings")
	TSubclassOf<APGPooledProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Pool Settings")
	int32 InitialPoolSize = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Pool Settings")
	int32 MaxPoolSize = 100;

private:
	// World 캐싱
	UPROPERTY()
	UWorld* CachedWorld = nullptr;

	// 풀 관리 배열
	UPROPERTY()
	TArray<APGPooledProjectile*> AvailableProjectiles;

	UPROPERTY()
	TArray<APGPooledProjectile*> ActiveProjectiles;

public:
	// UObject interface
	virtual UWorld* GetWorld() const override;
	virtual void BeginDestroy() override;
	
public:
	// 초기화
	void Initialize(UWorld* InWorld);
	
	// 투사체 요청/반환
	UFUNCTION(BlueprintCallable)
	class APGPooledProjectile* RequestProjectile();
	void ReturnProjectile(class APGPooledProjectile* Projectile);

	// 클래스 로딩 상태 확인
	UFUNCTION(BlueprintCallable)
	bool IsProjectileClassLoaded() const;
	
	// 풀 설정
	void SetProjectileClass(const FSoftClassPath& InClassPath);
	void SetProjectileClass(TSubclassOf<APGPooledProjectile> InClass) { ProjectileClass = InClass; }
	void SetPoolSize(int32 Initial, int32 Max) { InitialPoolSize = Initial; MaxPoolSize = Max; }

	// 풀 관리
	UFUNCTION(BlueprintCallable)
	void InitializePool();

	// 통계
	UFUNCTION(BlueprintCallable)
	int32 GetActiveCount() const { return ActiveProjectiles.Num(); }
	int32 GetAvailableCount() const { return AvailableProjectiles.Num(); }
	int32 GetTotalCount() const { return GetActiveCount() + GetAvailableCount(); }

	// 풀 정리
	void CleanupPool();

private:
	APGPooledProjectile* CreateNewProjectile();
	bool LoadProjectileClass();
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGDamageFloaterManager.generated.h"


enum class EPGDamageType : uint8;
class UPGUIDamageFloater;

USTRUCT(BlueprintType)
struct FDamageFloaterData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DamageAmount;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPGDamageType DamageType;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WorldLocation;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TextColor;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsCritical;
	
	// 추적할 대상 액터
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<AActor> TargetActor;
};

USTRUCT(BlueprintType)
struct FPGDamageFloaterPool
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<UPGUIDamageFloater*> Widgets;
};

/**
 * 
 */
UCLASS()
class PGUI_API UPGDamageFloaterManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	static TWeakObjectPtr<UPGDamageFloaterManager> WeakThis;

private:
	// 풀링된 플로터들
	UPROPERTY()
	TMap<EPGDamageType, FPGDamageFloaterPool> Pools;

	// 현재 활성화된 플로터들
	UPROPERTY()
	TArray<UPGUIDamageFloater*> ActiveFloaters;

	// 풀 설정
	UPROPERTY(EditAnywhere, Category = "PG|PoolSettings")
	int32 MaxPoolSize = 50;

	// 사용하지 않는 풀 제거 타이머
	UPROPERTY(EditAnywhere, Category = "PG|Performance")
	float CleanupInterval = 120.0f;
	
private:
	// 정리 타이머
	FTimerHandle CleanupTimerHandle;

public:
	static UPGDamageFloaterManager* Get();
	
public:
	UPGUIDamageFloater* GetPooledFloater(EPGDamageType DamageType);
	void ReturnFloaterToPool(EPGDamageType DamageType, UPGUIDamageFloater* Floater);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
public:
	void AddFloater(float DamageAmount,
		EPGDamageType DamageType,
		AActor* TargetActor,
		bool IsPlayer = false);

protected:
	// 플로터 정리 타이머
	UFUNCTION()
	void CleanupExpiredFloaters();
	
private:
	UPGUIDamageFloater* CreateFloaterWidget(EPGDamageType DamageType);
	void ClearPool();
};

#define PGDamageFloater() UPGDamageFloaterManager::Get()

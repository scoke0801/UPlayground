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
	UPROPERTY()
	TArray<UPGUIDamageFloater*> AvailableFloaters;
    
	UPROPERTY()
	TArray<UPGUIDamageFloater*> ActiveFloaters;

public:
	static UPGDamageFloaterManager* Get();
	
public:
	UPGUIDamageFloater* GetPooledFloater();
	void ReturnFloaterToPool(UPGUIDamageFloater* Floater);

public:
	void AddFloater(float DamageAmount,EPGDamageType DamageType, bool IsPlayer);
};

#define PGDamageFloater() UPGDamageFloaterManager::Get();
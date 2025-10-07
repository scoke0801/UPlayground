// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PGAnimNotify_SkillIndicator.generated.h"

class UPGSkillMontageController;

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGAnimNotify_SkillIndicator : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	int32 SkillIndicatorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|AnimNotify")
	float IndicatorDuration = 1.0f;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	float SpawnDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	FVector SpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	FVector DecalSize = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	FName TargetActorKeyName = FName("TargetActor");

	// 인디케이터 완료 후 스폰할 ProjectileID( 사용 안하고 직접 AnimNotify_Projectile도 가능 )
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	int32 ProjectileId = 0;
	
protected:
	UPROPERTY(Transient)
	AActor* CachedOwner = nullptr;

private:
	FVector CachedDirection;
	FVector CachedLocation;

private:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	AActor* GetTargetActor(AActor* OwnerActor) const;
	FVector GetGroundLocation(AActor* TargetActor) const;

	UFUNCTION()
	void OnIndicatorCompleted(APGSkillIndicator* Indicator);
};

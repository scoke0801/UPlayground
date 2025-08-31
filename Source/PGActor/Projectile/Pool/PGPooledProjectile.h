// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Projectile/PGProjectileBase.h"
#include "PGPooledProjectile.generated.h"

/**
 * 
 */
UCLASS()
class PGACTOR_API APGPooledProjectile : public APGProjectileBase
{
	GENERATED_BODY()

	friend class APGProjectilePool;
	
private:
	bool bInUse = false;

	UPROPERTY(Transient)
	class APGProjectilePool* OwnerPool = nullptr;
	
public:
	// 풀로 반환
	void ReturnToPool();

	// 상태 확인
	bool IsInUse() const { return bInUse; }

public:
	// 투사체 발사
	virtual void Fire(const FVector& StartLocation, const FVector& Direction, float Speed = 1000.0f, float InDamage = 10.0f) override;

	virtual void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;

	virtual void OnLifeTimeExpired() override;
};

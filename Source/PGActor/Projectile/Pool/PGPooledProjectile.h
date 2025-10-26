// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Projectile/PGProjectileBase.h"
#include "PGPooledProjectile.generated.h"

class UPGProjectilePool;

/**
 * 풀링되는 투사체
 */
UCLASS()
class PGACTOR_API APGPooledProjectile : public APGProjectileBase
{
	GENERATED_BODY()

	friend class UPGProjectilePool;
	
private:
	bool bInUse = false;

	// UObject 기반 Pool 참조
	UPROPERTY()
	UPGProjectilePool* OwnerPool = nullptr;
	
public:
	// 풀로 반환
	void ReturnToPool();

	// 상태 확인
	bool IsInUse() const { return bInUse; }

public:
	// 투사체 발사
	virtual void Fire(AActor* InShooterActor, const FVector& InStartLocation, const FVector& Direction, float Speed = 1000.0f, float InDamage = 10.0f) override;

	virtual void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;

	virtual void OnProjectileOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult& Hit) override;

	virtual void OnLifeTimeExpired() override;

	virtual void OnMaxDistanceReached() override;
	
protected:
	// Actor 생명주기
	virtual void BeginDestroy() override;
};
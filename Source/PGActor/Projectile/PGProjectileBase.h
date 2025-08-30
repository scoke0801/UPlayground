// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGShared/Shared/Enum/PGProjectileEnumType.h"
#include "PGProjectileBase.generated.h"


UCLASS()
class PGACTOR_API APGProjectileBase : public AActor
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UProjectileMovementComponent* MovementComponent;

	// 설정값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float LifeTime = 5.0f;

protected:
	FTimerHandle LifeTimeHandle;
	
	// 충돌 이벤트
	UPROPERTY(BlueprintAssignable)
	FPGOnProjectileHit OnHit;

public:
	APGProjectileBase();
	
public:
	// 투사체 발사
	UFUNCTION(BlueprintCallable)
	virtual void Fire(const FVector& StartLocation, const FVector& Direction, float Speed = 1000.0f, float InDamage = 10.0f);

protected:
	UFUNCTION()
	virtual void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, 
						UPrimitiveComponent* OtherComp, FVector NormalImpulse, 
						const FHitResult& Hit);

	virtual void OnLifeTimeExpired();
};

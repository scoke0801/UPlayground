// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/Actor.h"
#include "PGShared/Shared/Enum/PGProjectileEnumType.h"
#include "PGProjectileBase.generated.h"


UCLASS()
class PGACTOR_API APGProjectileBase : public AActor
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG")
	USceneComponent* Root;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "PG|Projectile")
	class UBoxComponent* ProjectileCollisionBox;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "PG|Components")
	class UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "PG|Components")
	class UProjectileMovementComponent* MovementComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "PG|Projectile")
	class UNiagaraComponent* ProjectileNiagaraComponent;
	
	// 설정값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Projectile")
	float Damage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Projectile")
	float LifeTime = 5.0f;

protected:
	UPROPERTY(Transient)
	FTimerHandle LifeTimeHandle;

	UPROPERTY(Transient)
	AActor* Shooter;
	
public:
	APGProjectileBase();
	
public:
	// 투사체 발사
	UFUNCTION(BlueprintCallable)
	virtual void Fire(AActor* InShooterActor, const FVector& StartLocation, const FVector& Direction, float Speed = 1000.0f, float InDamage = 10.0f);

protected:
	UFUNCTION()
	virtual void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, 
						UPrimitiveComponent* OtherComp, FVector NormalImpulse, 
						const FHitResult& Hit);
	
	UFUNCTION()
	virtual void OnProjectileOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep,
		const FHitResult& Hit);
	
	virtual void OnLifeTimeExpired();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/Actor.h"
#include "PGShared/Shared/Enum/PGProjectileEnumType.h"
#include "PGAreaOfEffectBase.generated.h"

/**
 * Area Of Effect 유형 스킬
 */
UCLASS()
class PGACTOR_API APGAreaOfEffectBase : public AActor
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG")
	USceneComponent* Root;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "PG|Projectile")
	class UBoxComponent* CollisionBox;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "PG|Projectile")
	class UNiagaraComponent* NiagaraComponent;
	
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
	APGAreaOfEffectBase();
	
public:
	static APGAreaOfEffectBase* Fire(AActor* InShooterActor, const FVector& StartLocation,
		int32 EffectId, float InDamage);

protected:
	UFUNCTION()
	virtual void OnEffectHit(UPrimitiveComponent* HitComp, AActor* OtherActor, 
						UPrimitiveComponent* OtherComp, FVector NormalImpulse, 
						const FHitResult& Hit);
	
	UFUNCTION()
	virtual void OnEffectOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep,
		const FHitResult& Hit);
	
	virtual void OnLifeTimeExpired();

private:
	void Fire(AActor* InShooterActor, const FVector& StartLocation, float InDamage);
};

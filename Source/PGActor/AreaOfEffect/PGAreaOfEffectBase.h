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
	
	// Legacy Particle System (UE4.27 Cascade... 구형 이펙트를 사용하게 될 경우도 있을 것 같아서)
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "PG|AOE")
	class UParticleSystemComponent* ParticleComponent;

protected:
	UPROPERTY(Transient)
	FTimerHandle LifeTimeHandle;

	UPROPERTY(Transient)
	AActor* Shooter;

private:
	float LifeTime = 5.0f;
	float DamageTickInterval = 5.0f;
	float Damage = 10.0f;
	
public:	APGAreaOfEffectBase();
	
public:
	static APGAreaOfEffectBase* Fire(AActor* InShooterActor, const FVector& StartLocation, int32 EffectId);

protected:
	UFUNCTION()
	virtual void OnEffectOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep,
		const FHitResult& Hit);
	
	UFUNCTION()
	void OnEffectEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int OtherBodyIndex);

	virtual void OnLifeTimeExpired();

private:
	void Fire(AActor* InShooterActor, const FVector& StartLocation);
};

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

private:
	/** Enemy 충돌 채널 */
	static constexpr ECollisionChannel EnemyCollisionChannel = ECC_GameTraceChannel1;
	
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

	// 최대 이동 거리 (0이면 거리 제한 없음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Projectile")
	float MaxTravelDistance = 0.0f;

protected:
	UPROPERTY(Transient)
	FTimerHandle LifeTimeHandle;

	UPROPERTY(Transient)
	AActor* Shooter;

	// 거리 추적용 변수들
	UPROPERTY(Transient)
	FVector StartLocation;

	UPROPERTY(Transient)
	float TraveledDistance;
	
public:
	APGProjectileBase();

protected:
	virtual void Tick(float DeltaTime) override;
	
public:
	// 투사체 발사
	UFUNCTION(BlueprintCallable)
	virtual void Fire(AActor* InShooterActor, const FVector& InStartLocation, const FVector& Direction, float Speed = 1000.0f, float InDamage = 10.0f);

	// 최대 이동 거리 설정
	UFUNCTION(BlueprintCallable)
	void SetMaxTravelDistance(float InMaxDistance) { MaxTravelDistance = InMaxDistance; }

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

	// 최대 거리 도달 시 호출
	virtual void OnMaxDistanceReached();

	// 거리 체크
	void CheckTravelDistance();
};

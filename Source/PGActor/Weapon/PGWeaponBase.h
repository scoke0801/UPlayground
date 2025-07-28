// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameFramework/Actor.h"
#include "PGShared/Shared/Structure/PlayerStructTypes.h"
#include "PGWeaponBase.generated.h"

class UBoxComponent;
DECLARE_DELEGATE_OneParam(FOnTargetInteractedDelegate, AActor*)

UCLASS()
class PGACTOR_API APGWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:
	FOnTargetInteractedDelegate OnWeaponHitTarget;
	FOnTargetInteractedDelegate OnWeaponPullTarget;
	
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG|Weapons")
	UStaticMeshComponent* WeaponMesh;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG|Weapons")
	UBoxComponent* WeaponCollisionBox;
	
private:
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "WeaponData")
	FPGPlayerWeaponData PlayerWeaponData;
	
public:	
	// Sets default values for this actor's properties
	APGWeaponBase();

public:
	UFUNCTION()
	virtual void OnCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnCollisionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	
	UFUNCTION(BlueprintCallable)
	void AssignGrantAbilitySpecHandles(const TArray<FGameplayAbilitySpecHandle>& InSpecHandles);

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAbilitySpecHandle> GetGrantedAbilitySpecHandles();

public:
	FORCEINLINE UBoxComponent* GetWeaponCollisionBox() const { return WeaponCollisionBox;}


};

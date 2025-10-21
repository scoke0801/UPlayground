// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameFramework/Actor.h"
#include "PGShared/Shared/Structure/PlayerStructTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGWeaponBase.generated.h"

class UBoxComponent;
DECLARE_DELEGATE_OneParam(FOnTargetInteractedDelegate, AActor*)

UENUM(BlueprintType)
enum class EPGWeaponMeshType : uint8
{
	StaticMesh		UMETA(DisplayName = "Static Mesh"),
	SkeletalMesh	UMETA(DisplayName = "Skeletal Mesh")
};

UCLASS()
class PGACTOR_API APGWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:
	FOnTargetInteractedDelegate OnWeaponHitTarget;
	FOnTargetInteractedDelegate OnWeaponPullTarget;
	
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG|Weapons")
	USceneComponent* Root;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG|Weapons")
	UStaticMeshComponent* StaticWeaponMesh;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG|Weapons")
	USkeletalMeshComponent* SkeletalWeaponMesh;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category= "PG|Weapons")
	UBoxComponent* WeaponCollisionBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG|Weapons")
	EPGWeaponMeshType WeaponMeshType = EPGWeaponMeshType::StaticMesh;

	// 무기 스탯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "PG|Weapon Stats")
	TMap<EPGStatType, int32> WeaponStats;
	
private:
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;
	
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

public:
	UFUNCTION(BlueprintCallable)
	void AssignGrantAbilitySpecHandles(const TArray<FGameplayAbilitySpecHandle>& InSpecHandles);

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAbilitySpecHandle> GetGrantedAbilitySpecHandles() const;
	
public:
	FORCEINLINE UBoxComponent* GetWeaponCollisionBox() const { return WeaponCollisionBox;}
	UMeshComponent* GetMeshComponent() const;
	
	// 무기 스탯 관련 함수
	UFUNCTION(BlueprintCallable, Category = "PG|Weapon Stats")
	int32 GetWeaponStat(EPGStatType StatType) const;
	
	UFUNCTION(BlueprintCallable, Category = "PG|Weapon Stats")
	const TMap<EPGStatType, int32>& GetWeaponStats() const;
	
#if WITH_EDITOR
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

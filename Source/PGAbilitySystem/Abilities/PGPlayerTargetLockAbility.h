

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PGPlayerGameplayAbility.h"
#include "PGPlayerTargetLockAbility.generated.h"

class UInputMappingContext;
class UPGActorWidgetBase;
/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGPlayerTargetLockAbility : public UPGPlayerGameplayAbility
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	float BoxTraceDistance = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	FVector TraceBoxSize = FVector(5000.0f, 5000.f, 300.f);
	
	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	TArray<TEnumAsByte<EObjectTypeQuery> > BoxTraceChannel;

	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	bool bShowPersistentDebugShape = false;

	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	TSubclassOf<UPGActorWidgetBase> TargetLockWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	float TargetLockRotationInterpSpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	float TargetLockMaxWalkSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	UInputMappingContext* TargetLockMappingContext;
	
	UPROPERTY(EditDefaultsOnly, Category ="PG|TargetLcok")
	float TargetLockCameraOffsetDistance = 20.0f;
	
private:
	UPROPERTY(Transient)
	UPGActorWidgetBase* DrawnTargetLockWidget= nullptr;
	
	UPROPERTY(Transient)
	TArray<AActor*> AvailableActorToLock;

	UPROPERTY(Transient)
	AActor* CurrentLockedActor = nullptr;

	UPROPERTY(Transient)
	FVector2D TargetLockWidgetSize = FVector2D::ZeroVector;
	
	UPROPERTY(Transient)
	float CachedMaxWalkSpeed = 0.f;
	
protected:
	void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION(BlueprintCallable)
	void OnTargetLockTick(float DeltaTime);

	UFUNCTION(BlueprintCallable)
	void SwitchTarget(const FGameplayTag& InSwitchDirectionTag);
	
private:
	void TryLockOnTarget();

	void GetAvailableActorsToLock();

	AActor* GetNearestTargetFromAvailableActors(const TArray<AActor*>& InAvailableActors);

	void GetAvailableActorAroundTarget(TArray<AActor*>& OutActorsOnLeft, TArray<AActor*>& OutActorOnRight);
	
	void DrawTargetLockWidget();

	void SetTargetLockWidgetPosition();
	void InitTargetLockMovement();
	void InitTArgetLockMappingContext();
	
	void CancelTargetLockAbility();
	void CleanUp();
	void ResetTargetLockMovement();
	void ResetTargetLockMappingContext();
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGEnemyPresentationComponent.generated.h"

/** Event-driven armor and audio; a short timer runs only while poses/charge change. */
UCLASS(ClassGroup=(PG), meta=(BlueprintSpawnableComponent))
class PGACTOR_API UPGEnemyPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPGEnemyPresentationComponent();
    void Initialize(class UPGEnemyPresentationData* InData);
    void SetGuarding(bool bEnabled);
    void BeginWindup(float Duration, float AimTrackingSeconds);
    void BeginRecovery();
    void ResetPresentation(bool bDead = false);
    bool PlayGuardImpact();
    void PlayHitRecoil(const FVector& WorldDirection, float Distance, float Duration);
    void SetDissolve(float Amount);
    UFUNCTION(BlueprintPure) bool IsAimLocked() const { return bAimLocked; }
    UFUNCTION(BlueprintPure) float GetExposureAlpha() const { return ExposureAlpha; }
    UFUNCTION(BlueprintPure) int32 GetArmorCount() const { return Armor.Num(); }

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UPGEnemyPresentationData> Data;
    UPROPERTY(Transient) TArray<TObjectPtr<class UStaticMeshComponent>> Armor;
    UPROPERTY(Transient) TArray<int32> ArmorIndices;
    UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> Materials;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> PreparedAssets;
    FTimerHandle UpdateTimer;
    FTimerHandle RecoilTimer;
    TWeakObjectPtr<class USkeletalMeshComponent> RecoilMesh;
    FVector RecoilOffset = FVector::ZeroVector;
    FVector RecoilPeak = FVector::ZeroVector;
    double RecoilStartedAt = 0;
    float RecoilSeconds = 0;
    void UpdateHitRecoil();
    void ClearHitRecoil();
    double WindupStartedAt = 0;
    double LastUpdateAt = 0;
    double NextGuardHitAt = 0;
    float WindupDuration = 0;
    float AimLockAfter = 0;
    float ExposureAlpha = 0;
    float TargetExposure = 0;
    bool bWindup = false;
    bool bGuarding = false;
    bool bAimLocked = false;
    bool bStopped = false;
    void StartUpdating();
    void UpdatePresentation();
    void ApplyAppearance(float Charge);
    void PlaySound(class USoundBase* Sound) const;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "PGPlayerAttackComponent.generated.h"

DECLARE_DELEGATE_OneParam(FPGPlayerAttackEnded, bool /* cancelled */);

// Owns the single logical clock, spatial queries and reversible movement state.
UCLASS()
class PGACTOR_API UPGPlayerAttackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPGPlayerAttackComponent();
    void PrepareLoadout();
    bool CanPrepare(const UPGPlayerSkillProfile* Profile, const class UAnimMontage* Montage, FString& Error) const;
    bool Start(const UPGPlayerSkillProfile* Profile, UAnimMontage* Montage, bool bRefundEligible, FPGPlayerAttackEnded OnEnded);
    void Stop(bool bNotify = false, bool bCancelled = true);
    bool IsRunning() const { return CastContext.IsValid(); }
    bool CanCancel(bool bDodge) const;
    float GetLogicalTime() const { return LogicalTime; }
    float GetExpectedSeconds() const;
    const TSharedPtr<FPGSkillCastContext>& GetCastContext() const { return CastContext; }
    // Geometry helper shared with deterministic boundary tests. Targets use capsule footprints.
    static bool ContainsTarget(const FPGPlayerHitPhase& Hit, const FVector& Origin, const FVector& Forward,
        const FVector& TargetFeet, float CapsuleRadius);

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FPGPlayerProfileLifecycleTest;
    UPROPERTY(Transient) TObjectPtr<UPGPlayerSkillProfile> ActiveProfile;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> PreparedLoadoutAssets;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraSystem> PreparedVFX;
    UPROPERTY(Transient) TObjectPtr<class USoundBase> PreparedSound;
    UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> SlashMesh;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> SlashMID;
    double SlashUntil = 0.;
    TSharedPtr<FPGSkillCastContext> CastContext;
    FPGPlayerAttackEnded Ended;
    float LogicalTime = 0.f;
    float Speed = 1.f;
    float SavedWalkSpeed = 0.f;
    uint8 SavedRootMotionMode = 0;
    FVector LockedForward = FVector::ForwardVector;
    TSet<int32> PresentedPhases;
    bool bAimLocked = false;
    void Advance(float Seconds);
    bool MoveBetween(float From, float To);
    void QueryHit(const FPGPlayerHitPhase& Hit);
    void PresentHit(const FPGPlayerHitPhase& Hit);
};

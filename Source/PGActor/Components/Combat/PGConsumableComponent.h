#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGShared/Shared/Message/Combat/PGConsumablePresentation.h"
#include "PGConsumableComponent.generated.h"

class UPGConsumableData;
class APGStageManager;

UCLASS(ClassGroup=(PG), meta=(BlueprintSpawnableComponent))
class PGACTOR_API UPGConsumableComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPGConsumableComponent();
    // Idempotent: profile/equipment refreshes cannot replenish encounter resources.
    void Initialize(UPGConsumableData* Definition);
    bool TryUse();
    FPGConsumableState GetState() const;
    float GetRemainingCooldown() const;
    void BeginStage(APGStageManager* Stage);
    void CompleteStage(APGStageManager* Stage);
    void OnOwnerDied();
    const UPGConsumableData* GetDefinition() const { return Data; }
    void DebugSetCharges(int32 Count);
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FPGConsumableLifecycleTest;
    UPROPERTY(Transient) TObjectPtr<UPGConsumableData> Data;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraSystem> PreparedVFX;
    UPROPERTY(Transient) TObjectPtr<class USoundBase> PreparedSound;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraComponent> ActiveVFX;
    mutable TWeakObjectPtr<APGStageManager> ActiveStage;
    FTimerHandle EffectTimer;
    int32 Charges = 0;
    int32 RestockedStage = INDEX_NONE;
    double CooldownUntil = 0.;
    double NoticeUntil = 0.;
    FText Notice;
    bool bUsing = false;
    bool bInitialized = false;
    bool bStartedStage = false;
    bool bEnded = false;
    bool CanUse(FText& Reason) const;
    void Publish();
    void StopEffect();
    void SetNotice(const FText& Text);
};

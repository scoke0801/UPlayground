#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "PGPlayerSkillProjectile.generated.h"

// Retains the original cast when its ability ends or the player starts another skill.
UCLASS()
class PGACTOR_API APGPlayerSkillProjectile : public AActor
{
    GENERATED_BODY()
public:
    APGPlayerSkillProjectile();
    void Initialize(const UPGPlayerSkillProfile* Profile, const FPGPlayerHitPhase& Phase,
        TSharedPtr<FPGSkillCastContext> Context, const FVector& Direction);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Visual;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> SlashMID;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraComponent> NiagaraSlash;
    UPROPERTY(Transient) TObjectPtr<UPGPlayerSkillProfile> VisualProfile;
    TSharedPtr<FPGSkillCastContext> CastContext;
    TSharedPtr<struct FPGSkillObservation> Observation;
    FPGPlayerHitPhase Hit;
    FVector Forward;
    float Speed = 0, RemainingRange = 0, RemainingTime = 0;
    float VisualLifetime = 1.f;
    TWeakObjectPtr<class APGStageManager> Stage;
    int32 StageId = 0;
    void Sweep(float Distance);
};

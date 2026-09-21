#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCombatFeedbackData.generated.h"
USTRUCT(BlueprintType)
struct PGDATA_API FPGImpactFeedback
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UNiagaraSystem> VFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class USoundBase> Sound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<class UCameraShakeBase> Shake;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="0.15")) float HitStop = .035f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="3")) float Intensity = 1.f;
};
UCLASS(BlueprintType)
class PGDATA_API UPGCombatFeedbackData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FPGImpactFeedback Normal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FPGImpactFeedback Heavy;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FPGImpactFeedback Critical;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="0.15")) float MaximumHitStop = .075f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float CameraShakeInterval = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="2")) float CriticalFloaterScale = 1.35f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor CriticalFloaterColor = FLinearColor(1.f,.65f,.1f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="0.15"))
    float HitStopSeconds = 0.035f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0"))
    float MinimumFeedbackInterval = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="3"))
    float CriticalIntensity = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<class UNiagaraSystem> ImpactVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<class USoundBase> ImpactSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<class UCameraShakeBase> CameraShake;
};

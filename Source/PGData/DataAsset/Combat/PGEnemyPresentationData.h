#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGEnemyPresentationData.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGEnemyArmorPiece
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Socket;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform GuardTransform;
    // Relative to the same bone: the shield drops and turns outward during recovery.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform RecoveryTransform;
};

/** Cosmetic only. Guard, damage and recovery authority remain in the existing GAS path. */
UCLASS(BlueprintType)
class PGDATA_API UPGEnemyPresentationData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FPGEnemyArmorPiece> Armor;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float PoseBlendSeconds = .18f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor GuardColor = FLinearColor(.08f, .55f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor WindupColor = FLinearColor(1.f, .22f, .04f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float GuardEmission = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float ExposedEmission = .05f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class USoundBase> WindupSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class USoundBase> AimLockSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class USoundBase> RecoverySound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class USoundBase> GuardHitSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class UNiagaraSystem> GuardHitVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float GuardHitVFXScale = .25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float GuardHitInterval = .12f;
};

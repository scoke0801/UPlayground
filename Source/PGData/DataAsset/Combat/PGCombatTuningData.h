#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCombatTuningData.generated.h"

UCLASS(BlueprintType)
class PGDATA_API UPGCombatTuningData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1"))
    float DefenseConstant = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1"))
    float BaseCriticalMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0"))
    float MinimumDamage = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perks", meta=(ClampMin="0", ClampMax="1"))
    float ExecutionHealthThreshold = .35f;
    UPROPERTY(EditAnywhere, Category="Roguelike", meta=(ClampMin="0.1")) float BleedInterval = .5f;
    UPROPERTY(EditAnywhere, Category="Roguelike", meta=(ClampMin="1")) int32 BleedTicks = 6;
    UPROPERTY(EditAnywhere, Category="Roguelike", meta=(ClampMin="1")) int32 BleedMaxStacks = 5;
    UPROPERTY(EditAnywhere, Category="Roguelike", meta=(ClampMin="1")) float ProcRadius = 350.f;
    UPROPERTY(EditAnywhere, Category="Roguelike", meta=(ClampMin="0.1")) float ShockCooldown = .8f;
    UPROPERTY(EditAnywhere, Category="Roguelike", meta=(ClampMin="0.1")) float FrenzySeconds = 3.f;
    UPROPERTY(EditAnywhere, Category="Roguelike", meta=(ClampMin="1")) int32 FrenzyMaxStacks = 10;
    UPROPERTY(EditAnywhere, Category="Roguelike") TSoftObjectPtr<class UNiagaraSystem> BleedVFX;
    UPROPERTY(EditAnywhere, Category="Roguelike") TSoftObjectPtr<class UNiagaraSystem> ShockVFX;
    UPROPERTY(EditAnywhere, Category="Roguelike") TSoftObjectPtr<class UNiagaraSystem> FrenzyVFX;
    UPROPERTY(EditAnywhere, Category="Keystone|Bleed", meta=(ClampMin="0", ClampMax="1")) float BleedRefundFraction = .35f;
    UPROPERTY(EditAnywhere, Category="Keystone|Shock", meta=(ClampMin="1", ClampMax="20")) int32 ShockFractureHits = 3;
    UPROPERTY(EditAnywhere, Category="Keystone|Shock", meta=(ClampMin="0.1")) float ShockStackSeconds = 4.f;
    UPROPERTY(EditAnywhere, Category="Keystone|Shock", meta=(ClampMin="0.1")) float ShockWeaknessSeconds = 4.f;
    UPROPERTY(EditAnywhere, Category="Keystone|Shock", meta=(ClampMin="0", ClampMax="1")) float ShockDefenseReduction = .3f;
    UPROPERTY(EditAnywhere, Category="Keystone|Frenzy", meta=(ClampMin="1")) float AfterimageRadius = 300.f;
    UPROPERTY(EditAnywhere, Category="Keystone|Frenzy", meta=(ClampMin="0")) float AfterimageAttackMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, Category="Keystone|Frenzy") TSoftObjectPtr<class UNiagaraSystem> AfterimageVFX;
    UPROPERTY(EditAnywhere, Category="Presentation", meta=(ClampMin="0.1")) float BuildProcDisplaySeconds = 1.5f;
};

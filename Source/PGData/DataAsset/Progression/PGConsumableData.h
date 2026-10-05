#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGConsumableData.generated.h"

/** Common healing supply. Runtime charges belong to the current encounter, not the equipment bag. */
UCLASS(BlueprintType)
class PGDATA_API UPGConsumableData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Potion") FName Id = TEXT("MoonlightPotion");
    UPROPERTY(EditAnywhere, Category="Potion") FText DisplayName = NSLOCTEXT("PG", "MoonlightPotion", "월빛 회복약");
    UPROPERTY(EditAnywhere, Category="Potion", meta=(ClampMin="0.01", ClampMax="1")) float HealFraction = .4f;
    UPROPERTY(EditAnywhere, Category="Potion", meta=(ClampMin="1", ClampMax="9")) int32 Capacity = 3;
    UPROPERTY(EditAnywhere, Category="Potion", meta=(ClampMin="0.1", ClampMax="120")) float CooldownSeconds = 8.f;
    UPROPERTY(EditAnywhere, Category="Supply") bool bRefillOnStageClear = true;
    UPROPERTY(EditAnywhere, Category="Supply") bool bHealOnStageClear = true;
    UPROPERTY(EditAnywhere, Category="Presentation", meta=(ClampMin="0", ClampMax="1")) float LowHealthFraction = .35f;
    UPROPERTY(EditAnywhere, Category="Presentation") TSoftObjectPtr<class UNiagaraSystem> HealVFX;
    UPROPERTY(EditAnywhere, Category="Presentation") TSoftObjectPtr<class USoundBase> HealSound;
    UPROPERTY(EditAnywhere, Category="Presentation") FLinearColor HealColor = FLinearColor(.12f, 1.f, .7f);
    UPROPERTY(EditAnywhere, Category="Presentation", meta=(ClampMin="0.1", ClampMax="3")) float EffectSeconds = .65f;
    UPROPERTY(EditAnywhere, Category="Presentation", meta=(ClampMin="0.01", ClampMax="5")) float EffectScale = .6f;

    bool IsValidDefinition() const
    {
        return !Id.IsNone() && !DisplayName.IsEmpty() && Capacity > 0 && Capacity <= 9 &&
            FMath::IsFinite(HealFraction) && HealFraction > 0.f && HealFraction <= 1.f &&
            FMath::IsFinite(CooldownSeconds) && CooldownSeconds >= .1f && CooldownSeconds <= 120.f &&
            FMath::IsFinite(LowHealthFraction) && LowHealthFraction >= 0.f && LowHealthFraction <= 1.f &&
            FMath::IsFinite(EffectSeconds) && EffectSeconds >= .1f && EffectSeconds <= 3.f &&
            FMath::IsFinite(EffectScale) && EffectScale >= .01f && EffectScale <= 5.f &&
            FMath::IsFinite(HealColor.R) && FMath::IsFinite(HealColor.G) && FMath::IsFinite(HealColor.B) && FMath::IsFinite(HealColor.A);
    }
};

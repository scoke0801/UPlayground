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
};
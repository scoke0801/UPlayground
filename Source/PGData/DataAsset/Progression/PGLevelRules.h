#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGLevelState.h"
#include "PGLevelRules.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGInvestmentRule
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="10000")) int32 AmountPerPoint = 2;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="10000")) int32 MaxPoints = 100;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGLevelStep
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int64 RequiredExperience = 100;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 GrantedPoints = 3;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGLevelRules
{
    GENERATED_BODY()
    // Each entry advances one level; the array length defines the maximum level.
    UPROPERTY(EditAnywhere) TArray<FPGLevelStep> Steps;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 StartingLevel = 1;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 StartingPoints = 0;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 KillExperience = 10;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 RewardExperience = 50;
    UPROPERTY(EditAnywhere) TMap<EPGStatType, FPGInvestmentRule> InvestmentRules;
    FPGLevelRules();
    bool IsValid() const;
    FPGLevelState InitialState() const;
    bool Validate(const FPGLevelState& State) const;
    bool AddExperience(FPGLevelState& State, int64 Amount) const;
    bool Invest(FPGLevelState& State, const TMap<EPGStatType, int32>& Draft) const;
    int32 InvestmentBonus(const FPGLevelState& State, EPGStatType Stat) const;
};

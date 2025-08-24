#pragma once
#include "PGDamageFloaterCurveData.generated.h"

UENUM(BlueprintType)
enum class EPGDamageFloaterCurveType : uint8
{
	Translation = 0,
	Scale,
	Opacity,
};

USTRUCT(BlueprintType)
struct FPGDamageFloaterCurveData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPGDamageFloaterCurveType CurveType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UCurveBase* Curve;
};


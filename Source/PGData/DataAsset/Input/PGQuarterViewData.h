#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGQuarterViewData.generated.h"
UCLASS(BlueprintType)
class PGDATA_API UPGQuarterViewData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator Rotation = FRotator(-55.f, -45.f, 0.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float Distance = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float MinDistance = 700.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float MaxDistance = 1600.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float LagSpeed = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float ZoomSpeed = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float AimTraceDistance = 100000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TEnumAsByte<ECollisionChannel> AimChannel = ECC_Visibility;
};
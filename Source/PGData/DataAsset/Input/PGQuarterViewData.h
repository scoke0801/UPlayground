#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGQuarterViewData.generated.h"
UCLASS(BlueprintType)
class PGDATA_API UPGQuarterViewData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dodge") bool bDodgeFollowsMovement = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator Rotation = FRotator(-55.f, -45.f, 0.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float Distance = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float MinDistance = 700.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float MaxDistance = 1600.f;
    // MinDistance marks the start of the close-up transition; combat framing stays unchanged beyond it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CloseUp", meta=(ClampMin="100")) float CloseUpDistance = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CloseUp", meta=(ClampMin="-80", ClampMax="-5")) float CloseUpPitch = -10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CloseUp") float CloseUpFocusHeight = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CloseUp", meta=(ClampMin="0.1")) float ZoomInterpSpeed = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pitch", meta=(ClampMin="-89", ClampMax="-5")) float MinPitch = -75.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pitch", meta=(ClampMin="-89", ClampMax="-5")) float MaxPitch = -5.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D", meta=(ClampMin="100")) float ActionDistance = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D", meta=(ClampMin="100")) float ActionMinDistance = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D", meta=(ClampMin="100")) float ActionMaxDistance = 900.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D", meta=(ClampMin="-89", ClampMax="89")) float ActionPitch = -20.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D", meta=(ClampMin="-89", ClampMax="89")) float ActionMinPitch = -80.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D", meta=(ClampMin="-89", ClampMax="89")) float ActionMaxPitch = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D") float ActionFocusHeight = 15.f;
    // Sphere clearance for the near view plane against ground and walls (centimeters).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collision", meta=(ClampMin="12")) float CameraProbeRadius = 24.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collision", meta=(ClampMin="0")) float CameraMaxLagDistance = 60.f;
    // Hide the local body before the near plane reaches it; extra exit clearance prevents flicker.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collision", meta=(ClampMin="0")) float CameraBodyClearance = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collision", meta=(ClampMin="0")) float CameraBodyHideHysteresis = 15.f;
    // Fade over this distance outside the body clearance, using the final collision-corrected view.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action3D", meta=(ClampMin="1", Units="cm")) float CameraBodyFadeDistance = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float LagSpeed = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float ZoomSpeed = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float AimTraceDistance = 100000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TEnumAsByte<ECollisionChannel> AimChannel = ECC_Visibility;
};

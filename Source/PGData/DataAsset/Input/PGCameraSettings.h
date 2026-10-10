#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PGCameraSettings.generated.h"

UENUM(BlueprintType)
enum class EPGCameraMode : uint8
{
    QuarterView UMETA(DisplayName="쿼터뷰"),
    Action3D UMETA(DisplayName="3D 액션")
};

/** Machine-local preferences, independent of character/run saves. */
UCLASS(Config=GameUserSettings, BlueprintType)
class PGDATA_API UPGCameraSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Camera") EPGCameraMode CameraMode = EPGCameraMode::QuarterView;
    EPGCameraMode GetCameraMode() const
    {
        return CameraMode == EPGCameraMode::Action3D ? EPGCameraMode::Action3D : EPGCameraMode::QuarterView;
    }
};

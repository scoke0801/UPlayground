#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "PGCombatCameraShake.generated.h"

UCLASS(EditInlineNew)
class PGACTOR_API UPGCombatShakePattern : public UCameraShakePattern
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="PG|Feedback", meta=(ClampMin="0.01", ClampMax="0.3")) float Duration = .12f;
    UPROPERTY(EditAnywhere, Category="PG|Feedback", meta=(ClampMin="0")) float Amplitude = 2.f;
private:
    float Elapsed = 0;
    virtual void GetShakePatternInfoImpl(FCameraShakeInfo& Info) const override { Info.Duration = FCameraShakeDuration(Duration); }
    virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override { Elapsed = 0; }
    virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& Result) override;
    virtual bool IsFinishedImpl() const override { return Elapsed >= Duration; }
    virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override { Elapsed = Duration; }
};
UCLASS(Blueprintable)
class PGACTOR_API UPGCombatCameraShake : public UCameraShakeBase
{
    GENERATED_BODY()
public:
    UPGCombatCameraShake(const FObjectInitializer& ObjectInitializer);
};

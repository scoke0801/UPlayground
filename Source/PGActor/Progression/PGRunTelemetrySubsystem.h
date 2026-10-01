#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PGRunTelemetrySubsystem.generated.h"

struct FPGStageTelemetry
{
    int32 Seed = 0;
    int32 Stage = 0;
    bool bAssisted = false;
    double CombatSeconds = 0.;
    double DirectDamage = 0.;
    double SecondaryDamage = 0.;
    double DamageTaken = 0.;
    TArray<int32> Rewards;
    FString Outcome = TEXT("Interrupted");
};

// Local, observational data only. Never grants progression or commits a profile.
UCLASS()
class PGACTOR_API UPGRunTelemetrySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    static UPGRunTelemetrySubsystem* Get(const UObject* Context);
    virtual void Deinitialize() override;
    void StartStage(int32 Seed, int32 Stage, bool bAssisted);
    void StartWave();
    void EndWave();
    void EndStage(const FString& Outcome);
    void MarkAssisted();
    void RecordReward(int32 RewardId);
    // Capture this before GAS executes: a lethal hit can synchronously end a stage.
    int32 GetSampleIndex() const { return ActiveSample; }
    void RecordDamage(int32 Sample, float Applied, bool bPlayerSource, bool bPlayerTarget, bool bSecondary);
private:
    friend class FPGRunTelemetryTest;
    TArray<FPGStageTelemetry> Samples;
    int32 ActiveSample = INDEX_NONE;
    double WaveStartedAt = -1.;
    FString OutputPath;
    void Flush() const;
};

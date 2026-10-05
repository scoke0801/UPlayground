#include "PGRunTelemetrySubsystem.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"

UPGRunTelemetrySubsystem* UPGRunTelemetrySubsystem::Get(const UObject* Context)
{
    UWorld* World = Context ? Context->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UPGRunTelemetrySubsystem>() : nullptr;
}

void UPGRunTelemetrySubsystem::StartStage(int32 Seed, int32 Stage, bool bAssisted)
{
    EndWave();
    FPGStageTelemetry Sample;
    Sample.Seed = Seed; Sample.Stage = Stage; Sample.bAssisted = bAssisted;
    ActiveSample = Samples.Add(MoveTemp(Sample));
    if (OutputPath.IsEmpty())
        OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RunTelemetry"), FGuid::NewGuid().ToString() + TEXT(".json"));
    UE_LOG(LogTemp, Log, TEXT("PGRun stage=%d seed=%d assisted=%d telemetry=%s"), Stage, Seed, bAssisted, *OutputPath);
}

void UPGRunTelemetrySubsystem::StartWave()
{
    if (Samples.IsValidIndex(ActiveSample) && WaveStartedAt < 0.) WaveStartedAt = GetWorld()->GetTimeSeconds();
}

void UPGRunTelemetrySubsystem::EndWave()
{
    if (Samples.IsValidIndex(ActiveSample) && WaveStartedAt >= 0.)
        Samples[ActiveSample].CombatSeconds += FMath::Max(0., GetWorld()->GetTimeSeconds() - WaveStartedAt);
    WaveStartedAt = -1.;
}

void UPGRunTelemetrySubsystem::EndStage(const FString& Outcome)
{
    EndWave();
    if (!Samples.IsValidIndex(ActiveSample)) return;
    Samples[ActiveSample].Outcome = Outcome;
    // Damage settlement completes after death delegates. Flush on the next tick
    // and again on world teardown so the final lethal hit is included.
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { Flush(); }));
}

void UPGRunTelemetrySubsystem::MarkAssisted()
{
    for (auto& Sample : Samples) Sample.bAssisted = true;
}

void UPGRunTelemetrySubsystem::RecordReward(int32 RewardId)
{
    if (Samples.IsValidIndex(ActiveSample) && RewardId > 0) Samples[ActiveSample].Rewards.Add(RewardId);
    Flush();
}

void UPGRunTelemetrySubsystem::RecordDamage(int32 Sample, float Applied, bool bPlayerSource, bool bPlayerTarget, bool bSecondary)
{
    if (!Samples.IsValidIndex(Sample) || !FMath::IsFinite(Applied) || Applied <= 0.f) return;
    auto& Entry = Samples[Sample];
    if (bPlayerSource && !bPlayerTarget)
    {
        if (bSecondary) Entry.SecondaryDamage += Applied;
        else Entry.DirectDamage += Applied;
    }
    if (bPlayerTarget && !bPlayerSource) Entry.DamageTaken += Applied;
}

void UPGRunTelemetrySubsystem::Flush() const
{
    if (OutputPath.IsEmpty() || Samples.IsEmpty()) return;
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("schema_version"), 1);
    Root->SetStringField(TEXT("world"), GetWorld()->GetName());
    TArray<TSharedPtr<FJsonValue>> Stages;
    for (const auto& Sample : Samples)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("run_seed"), Sample.Seed);
        Row->SetNumberField(TEXT("stage"), Sample.Stage);
        Row->SetBoolField(TEXT("assisted"), Sample.bAssisted);
        Row->SetStringField(TEXT("outcome"), Sample.Outcome);
        Row->SetNumberField(TEXT("combat_seconds"), Sample.CombatSeconds);
        Row->SetNumberField(TEXT("direct_damage"), Sample.DirectDamage);
        Row->SetNumberField(TEXT("secondary_damage"), Sample.SecondaryDamage);
        Row->SetNumberField(TEXT("damage_taken"), Sample.DamageTaken);
        Row->SetNumberField(TEXT("potion_uses"), Sample.PotionUses);
        Row->SetNumberField(TEXT("potion_healing"), Sample.PotionHealing);
        Row->SetNumberField(TEXT("potion_overheal"), Sample.PotionOverheal);
        Row->SetNumberField(TEXT("potions_at_death"), Sample.PotionsAtDeath);
        Row->SetNumberField(TEXT("potion_cooldown_at_death"), Sample.PotionCooldownAtDeath);
        TArray<TSharedPtr<FJsonValue>> Rewards;
        for (int32 Id : Sample.Rewards) Rewards.Add(MakeShared<FJsonValueNumber>(Id));
        Row->SetArrayField(TEXT("selected_rewards"), Rewards);
        Stages.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("stages"), Stages);
    FString Json;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
    if (!FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        UE_LOG(LogTemp, Warning, TEXT("PGRun telemetry write failed: %s"), *OutputPath);
}

void UPGRunTelemetrySubsystem::Deinitialize()
{
    EndWave();
    Flush();
    Super::Deinitialize();
}

void UPGRunTelemetrySubsystem::RecordPotion(float Requested, float Applied)
{
    if (!Samples.IsValidIndex(ActiveSample) || !FMath::IsFinite(Requested) || !FMath::IsFinite(Applied) || Requested <= 0 || Applied <= 0) return;
    auto& Sample = Samples[ActiveSample];
    ++Sample.PotionUses;
    Sample.PotionHealing += Applied;
    Sample.PotionOverheal += FMath::Max(0.f, Requested - Applied);
}

void UPGRunTelemetrySubsystem::RecordPotionAtDeath(int32 Charges, float Cooldown)
{
    if (!Samples.IsValidIndex(ActiveSample)) return;
    Samples[ActiveSample].PotionsAtDeath = Charges;
    Samples[ActiveSample].PotionCooldownAtDeath = Cooldown;
}

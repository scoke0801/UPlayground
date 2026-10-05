#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

static TAutoConsoleVariable<int32> CVarPGSkillObserve(TEXT("pg.Skill.Observe"), 0,
    TEXT("Record optional cast/input/direct-hit QA telemetry. Does not verify physical input."));

TSharedPtr<FPGSkillObservation> UPGAbilitySystemComponent::BeginSkillObservation(int32 SkillID,
    const TSharedPtr<FPGSkillCastContext>& Context)
{
    if (!CVarPGSkillObserve.GetValueOnGameThread() || !GetWorld() || !GetAvatarActor()) return nullptr;
    auto Observation = MakeShared<FPGSkillObservation>();
    Observation->SkillID = SkillID;
    Observation->ProfileCast = Context;
    if (Context) { Observation->CastId = Context->CastId; Context->Observation = Observation; }
    Observation->InputAt = ObservedInputAt;
    Observation->Started = GetWorld()->GetTimeSeconds();
    Observation->Origin = GetAvatarActor()->GetActorLocation();
    ActiveObservation = Observation;
    UE_LOG(LogTemp, Display, TEXT("PGSkillMetric BEGIN cast=%s skill=%d world=%.6f input=%.6f profile=%d"),
        *Observation->CastId.ToString(), SkillID, Observation->Started, Observation->InputAt, Context.IsValid());
    return Observation;
}

void UPGAbilitySystemComponent::EndSkillObservation(const TSharedPtr<FPGSkillObservation>& Observation, bool bCancelled)
{
    if (!Observation || Observation->bEnded) return;
    if (Observation->Dependents > 0)
    {
        Observation->bEndRequested = true; Observation->bCancelled = bCancelled;
        if (ActiveObservation == Observation) ActiveObservation.Reset();
        return;
    }
    Observation->bEnded = true;
    const auto Context = Observation->ProfileCast;
    const float Displacement = GetAvatarActor() ? FVector::Dist2D(Observation->Origin, GetAvatarActor()->GetActorLocation()) : 0.f;
    UE_LOG(LogTemp, Display, TEXT("PGSkillMetric END cast=%s skill=%d world=%.6f cancelled=%d displacement=%.3f hits=%d damage=%.3f queries=%d frenzy=%d shock=%d refund=%d"),
        *Observation->CastId.ToString(), Observation->SkillID, GetWorld() ? GetWorld()->GetTimeSeconds() : Observation->Started,
        bCancelled, Displacement, Observation->Hits, Observation->DirectDamage, Context ? Context->SpatialQueries : -1,
        Context ? Context->FrenzyGranted : -1, Context ? int32(Context->bShockUsed) : -1, Context ? int32(Context->bRefundUsed) : -1);
    if (ActiveObservation == Observation) ActiveObservation.Reset();
}

FString UPGAbilitySystemComponent::GetObservedCastId() const
{
    return ActiveObservation ? ActiveObservation->CastId.ToString() : TEXT("none");
}

void UPGAbilitySystemComponent::RecordObservedHit(const TSharedPtr<FPGSkillObservation>& Observation,
    AActor* Target, int32 Phase, float Damage, const FVector& Origin, const FVector& Forward)
{
    if (!Observation || Damage <= 0.f || !IsValid(Target) || !GetWorld()) return;
    ++Observation->Hits;
    Observation->DirectDamage += Damage;
    const bool bBehind = FVector::DotProduct((Target->GetActorLocation() - Origin).GetSafeNormal2D(), Forward) < 0.f;
    UE_LOG(LogTemp, Display, TEXT("PGSkillMetric HIT cast=%s skill=%d phase=%d target=%s world=%.6f damage=%.3f behind=%d"),
        *Observation->CastId.ToString(), Observation->SkillID, Phase, *Target->GetName(), GetWorld()->GetTimeSeconds(), Damage, bBehind);
}

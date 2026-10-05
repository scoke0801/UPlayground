#include "PGPlayerSkillProfile.h"
#include "Misc/DataValidation.h"

bool UPGPlayerSkillProfile::Validate(int32 ExpectedSkillID, FString& Error) const
{
    const auto Fail = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    const auto TimeValid = [this](float Time) { return FMath::IsFinite(Time) && Time >= 0.f && Time <= Duration; };
    if (SkillID <= 0 || SkillID != ExpectedSkillID) return Fail(TEXT("SkillID mismatch"));
    if (!FMath::IsFinite(NiagaraReferenceRadius) || NiagaraReferenceRadius < 1.f ||
        !FMath::IsFinite(NiagaraReferenceDuration) || NiagaraReferenceDuration < .05f || NiagaraReferenceDuration > 5.f ||
        NiagaraRotation.ContainsNaN()) return Fail(TEXT("Invalid Niagara authoring units"));
    if (!FMath::IsFinite(SlashDuration) || SlashDuration < .05f || SlashDuration > .5f ||
        !FMath::IsFinite(SlashWidth) || SlashWidth < .02f || SlashWidth > .4f ||
        !FMath::IsFinite(SlashIntensity) || SlashIntensity < 0.f || SlashIntensity > 10.f ||
        !FMath::IsFinite(SlashHeight) || SlashHeight < 0.f || SlashHeight > 150.f ||
        !FMath::IsFinite(SlashTint.R) || !FMath::IsFinite(SlashTint.G) || !FMath::IsFinite(SlashTint.B) ||
        SlashTint.R < 0.f || SlashTint.G < 0.f || SlashTint.B < 0.f)
        return Fail(TEXT("Invalid slash presentation"));
    if (!FMath::IsFinite(Duration) || Duration <= 0.f || Duration > 10.f ||
        !TimeValid(AimLock) || !TimeValid(DodgeCancel) || !TimeValid(AttackCancel) || !TimeValid(EarlyDodgeUntil) || EarlyDodgeUntil > DodgeCancel ||
        !FMath::IsFinite(ProjectileSpeed) || ProjectileSpeed <= 0 || ProjectileSpeed > 10000 ||
        !FMath::IsFinite(ProjectileRange) || ProjectileRange <= 0 || ProjectileRange > 5000 ||
        !FMath::IsFinite(ProjectileLifetime) || ProjectileLifetime <= 0 || ProjectileLifetime > 5 ||
        !FMath::IsFinite(AttackSpeed) || AttackSpeed < .75f || AttackSpeed > 1.75f ||
        FrenzyPerCastCap < 0 || FrenzyPerCastCap > 3) return Fail(TEXT("Invalid clock/cancel/speed/proc cap"));
    TSet<FName> Segments;
    float LastEnd = 0.f;
    for (const auto& Move : MovementSegments)
    {
        if (Move.SegmentId.IsNone() || Segments.Contains(Move.SegmentId) || !TimeValid(Move.Start) ||
            !TimeValid(Move.End) || Move.End <= Move.Start || Move.Start < LastEnd ||
            !FMath::IsFinite(Move.Distance) || Move.Distance < 0.f || Move.Distance > 1500.f ||
            !FMath::IsFinite(Move.WalkSpeedRatio) || Move.WalkSpeedRatio < 0.f || Move.WalkSpeedRatio > 1.f ||
            (Move.Mode != EPGPlayerMoveMode::ForwardSweep && Move.Mode != EPGPlayerMoveMode::Walk && Move.Mode != EPGPlayerMoveMode::GroundLeap))
            return Fail(TEXT("Invalid/duplicate/overlapping movement segment"));
        Segments.Add(Move.SegmentId); LastEnd = Move.End;
    }
    if (HitPhases.IsEmpty()) return Fail(TEXT("No hit phases"));
    TSet<int32> Phases;
    float LastStart = -1.f;
    for (const auto& Hit : HitPhases)
    {
        if (Hit.PhaseId < 0 || Phases.Contains(Hit.PhaseId) || !TimeValid(Hit.Start) || !TimeValid(Hit.End) ||
            Hit.End < Hit.Start || Hit.Start < LastStart ||
            !FMath::IsFinite(Hit.Radius) || Hit.Radius <= 0.f || Hit.Radius > 1500.f ||
            !FMath::IsFinite(Hit.FullAngleDegrees) || Hit.FullAngleDegrees <= 0.f || Hit.FullAngleDegrees > 360.f ||
            !FMath::IsFinite(Hit.HeightTolerance) || Hit.HeightTolerance <= 0.f ||
            !FMath::IsFinite(Hit.DamageMultiplier) || Hit.DamageMultiplier < 0.f ||
            !FMath::IsFinite(Hit.HitStopSeconds) || Hit.HitStopSeconds < 0.f || Hit.HitStopSeconds > .15f ||
            (Hit.Shape != EPGPlayerHitShape::Fan && Hit.Shape != EPGPlayerHitShape::Disc && Hit.Shape != EPGPlayerHitShape::Projectile) ||
            (!Hit.MovementSegmentId.IsNone() && !Segments.Contains(Hit.MovementSegmentId)))
            return Fail(TEXT("Invalid/duplicate hit phase or missing movement reference"));
        Phases.Add(Hit.PhaseId); LastStart = Hit.Start;
    }
    if (PoseKeys.Num() < 2 || PoseKeys[0].Time != 0.f || PoseKeys[0].MontageSeconds < 0.f ||
        !FMath::IsNearlyEqual(PoseKeys.Last().Time, Duration)) return Fail(TEXT("Pose mapping must span the logical duration"));
    for (int32 Index = 0; Index < PoseKeys.Num(); ++Index)
    {
        const auto& Key = PoseKeys[Index];
        if (!TimeValid(Key.Time) || !FMath::IsFinite(Key.MontageSeconds) || Key.MontageSeconds < 0.f ||
            (Index && (Key.Time <= PoseKeys[Index - 1].Time || Key.MontageSeconds <= PoseKeys[Index - 1].MontageSeconds)))
            return Fail(TEXT("Pose mapping must be finite and strictly increasing"));
    }
    Error.Reset(); return true;
}

float UPGPlayerSkillProfile::GetMontagePosition(float Time) const
{
    // Preserve every authored hit pose, but share a positive tangent at each key.
    // Piecewise linear remapping snapped immediately between slow wind-up and up to
    // 9x recovery playback. Monotone Hermite interpolation keeps velocity continuous
    // without seeking backwards or overshooting a neighbouring pose.
    const auto Slope = [this](int32 Right)
    {
        const auto& A = PoseKeys[Right - 1]; const auto& B = PoseKeys[Right];
        return (B.MontageSeconds - A.MontageSeconds) / (B.Time - A.Time);
    };
    const auto Tangent = [this, &Slope](int32 Key)
    {
        if (Key == 0) return Slope(1);
        if (Key == PoseKeys.Num() - 1) return Slope(Key);
        const float Left = Slope(Key), Right = Slope(Key + 1);
        return Left > 0.f && Right > 0.f ? 2.f * Left * Right / (Left + Right) : 0.f;
    };
    for (int32 Index = 1; Index < PoseKeys.Num(); ++Index)
        if (Time <= PoseKeys[Index].Time)
        {
            const auto& A = PoseKeys[Index - 1]; const auto& B = PoseKeys[Index];
            const float Span = B.Time - A.Time;
            const float Alpha = FMath::Clamp((Time - A.Time) / Span, 0.f, 1.f);
            return FMath::Clamp(FMath::CubicInterp(A.MontageSeconds, Tangent(Index - 1) * Span,
                B.MontageSeconds, Tangent(Index) * Span, Alpha), A.MontageSeconds, B.MontageSeconds);
        }
    return PoseKeys.IsEmpty() ? 0.f : PoseKeys.Last().MontageSeconds;
}
#if WITH_EDITOR
EDataValidationResult UPGPlayerSkillProfile::IsDataValid(FDataValidationContext& Context) const
{
    FString Error;
    if (!Validate(SkillID, Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
    return EDataValidationResult::Valid;
}
#endif

#include "PGPlayerSkillProfile.h"
#include "Misc/DataValidation.h"

bool UPGPlayerSkillProfile::Validate(int32 ExpectedSkillID, FString& Error) const
{
    const auto Fail = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    const auto TimeValid = [this](float Time) { return FMath::IsFinite(Time) && Time >= 0.f && Time <= Duration; };
    if (SkillID <= 0 || SkillID != ExpectedSkillID) return Fail(TEXT("SkillID mismatch"));
    if (!FMath::IsFinite(Duration) || Duration <= 0.f || Duration > 10.f ||
        !TimeValid(AimLock) || !TimeValid(DodgeCancel) || !TimeValid(AttackCancel) ||
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
            (Move.Mode != EPGPlayerMoveMode::ForwardSweep && Move.Mode != EPGPlayerMoveMode::Walk))
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
            (Hit.Shape != EPGPlayerHitShape::Fan && Hit.Shape != EPGPlayerHitShape::Disc) ||
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
    for (int32 Index = 1; Index < PoseKeys.Num(); ++Index)
        if (Time <= PoseKeys[Index].Time)
        {
            const auto& A = PoseKeys[Index - 1]; const auto& B = PoseKeys[Index];
            return FMath::Lerp(A.MontageSeconds, B.MontageSeconds, FMath::Clamp((Time - A.Time) / (B.Time - A.Time), 0.f, 1.f));
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

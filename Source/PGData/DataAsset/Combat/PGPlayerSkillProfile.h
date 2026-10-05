#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGShared/Shared/Combat/PGSkillCastContext.h"
#include "PGPlayerSkillProfile.generated.h"

UENUM(BlueprintType)
enum class EPGPlayerHitShape : uint8 { Fan, Disc, Projectile };
UENUM(BlueprintType)
enum class EPGPlayerMoveMode : uint8 { ForwardSweep, Walk, GroundLeap };

USTRUCT(BlueprintType)
struct PGDATA_API FPGPlayerMovementSegment
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SegmentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Start = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float End = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EPGPlayerMoveMode Mode = EPGPlayerMoveMode::ForwardSweep;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Distance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WalkSpeedRatio = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEndCastOnBlock = false;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGPlayerHitPhase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PhaseId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Start = .16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float End = .22f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EPGPlayerHitShape Shape = EPGPlayerHitShape::Fan;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Radius = 250.f;
    // Full angle, unlike enemy pattern HalfAngleDegrees.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FullAngleDegrees = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeightTolerance = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWallOcclusion = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageMultiplier = .9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHeavyImpact = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HitStopSeconds = .025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MovementSegmentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FPGHitProcPolicy ProcPolicy;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGPlayerPoseKey
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Time = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MontageSeconds = 0.f;
};

// Opt-in player data. Enemy pattern validation and legacy row fields are independent.
UCLASS(BlueprintType)
class PGDATA_API UPGPlayerSkillProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SkillID = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = .48f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimLock = .10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DodgeCancel = .22f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackCancel = .30f;
    // Optional pre-takeoff cancel interval; zero disables it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float EarlyDodgeUntil = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileSpeed = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileRange = 1000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileLifetime = .7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackSpeed = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FrenzyPerCastCap = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGPlayerHitPhase> HitPhases;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGPlayerMovementSegment> MovementSegments;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGPlayerPoseKey> PoseKeys;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UNiagaraSystem> SlashVFX;
    // Optional caster-side swing at projectile release; SlashVFX travels with the projectile.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Niagara") TSoftObjectPtr<class UNiagaraSystem> ProjectileSwingVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Niagara", meta=(ClampMin="1", ClampMax="1500")) float ProjectileSwingRadius = 250.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class USoundBase> SwingSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UMaterialInterface> SlashMaterial;
    // Presentation only: the profile clock also freezes the slash during hit-stop.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation") FLinearColor SlashTint = FLinearColor(.18f, .8f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation", meta=(ClampMin="0.05", ClampMax="0.5")) float SlashDuration = .22f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation", meta=(ClampMin="0.02", ClampMax="0.4")) float SlashWidth = .16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation", meta=(ClampMin="0", ClampMax="10")) float SlashIntensity = 2.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation", meta=(ClampMin="0", ClampMax="150")) float SlashHeight = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation") bool bReverseSlash = false;
    // Authoring units of SlashVFX; runtime scales its footprint and simulation age.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Niagara", meta=(ClampMin="1")) float NiagaraReferenceRadius = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Niagara", meta=(ClampMin="0.05", ClampMax="5")) float NiagaraReferenceDuration = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Niagara") FRotator NiagaraRotation = FRotator::ZeroRotator;

    bool Validate(int32 ExpectedSkillID, FString& Error) const;
    float GetMontagePosition(float Time) const;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

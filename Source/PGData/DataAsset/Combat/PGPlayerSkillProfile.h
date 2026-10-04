#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGShared/Shared/Combat/PGSkillCastContext.h"
#include "PGPlayerSkillProfile.generated.h"

UENUM(BlueprintType)
enum class EPGPlayerHitShape : uint8 { Fan, Disc };
UENUM(BlueprintType)
enum class EPGPlayerMoveMode : uint8 { ForwardSweep, Walk };

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackSpeed = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FrenzyPerCastCap = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGPlayerHitPhase> HitPhases;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGPlayerMovementSegment> MovementSegments;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGPlayerPoseKey> PoseKeys;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UNiagaraSystem> SlashVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class USoundBase> SwingSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UMaterialInterface> SlashMaterial;

    bool Validate(int32 ExpectedSkillID, FString& Error) const;
    float GetMontagePosition(float Time) const;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

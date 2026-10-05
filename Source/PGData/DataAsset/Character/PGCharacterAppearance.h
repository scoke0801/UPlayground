#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PGCharacterAppearance.generated.h"

/** Finger-only local rotation offset from the target mesh reference pose. */
USTRUCT(BlueprintType)
struct PGDATA_API FPGAppearanceGripFinger
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Bone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator ReferenceRotationOffset = FRotator::ZeroRotator;
};

/** Explicit equipment attachment; absent/invalid entries use the reference-pose anchor. */
USTRUCT(BlueprintType)
struct PGDATA_API FPGAppearanceGripProfile
{
    GENERATED_BODY()
    // Reuses the carried weapon tag (Weapon.Sword, etc.). No parallel weapon taxonomy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag WeaponTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceSocket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TargetSocket;
    // Local to TargetSocket on the visible mesh. Translation is in imported bone units,
    // transformed by the bone/component scale; never divide all P09 offsets by 100.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Target bone/socket local transform, including bone and component scale. Translation is NOT world centimeters."))
    FTransform GripOffset = FTransform::Identity;
    // Evaluated after retargeting, only while this grip is actually equipped.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPGAppearanceGripFinger> Fingers;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGAppearancePart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class USkeletalMesh> Mesh;
    // Same-skeleton clothing follows the leader pose. Independent hair follows a bone.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AttachBone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform RelativeTransform;
};

/** Cosmetic identity only: abilities, collision, stats and combat clocks stay on the owner. */
UCLASS(BlueprintType)
class PGDATA_API UPGCharacterAppearance : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TSoftObjectPtr<class UTexture2D> Portrait;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class USkeletalMesh> SourceMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowedClasses="/Script/IKRig.IKRetargeter")) FSoftObjectPath Retargeter;
    // Enable for imported rigs whose retarget output already contains root scale (P09).
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bReconstructScaledTranslations = false;
    // Relative to the character's animation-driving mesh, preserving its capsule offset.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform MeshTransform;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName HeadBone;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector HeadForwardAxis = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector HeadRightAxis = FVector::RightVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FPGAppearancePart> Parts;
    // Map gameplay skeleton bones to the visible rig for weapon/sheath attachment.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, FName> EquipmentBones;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Polish|Grip") TArray<FPGAppearanceGripProfile> GripProfiles;
};

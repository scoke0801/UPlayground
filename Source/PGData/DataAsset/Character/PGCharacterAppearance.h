#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCharacterAppearance.generated.h"

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
};

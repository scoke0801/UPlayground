#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "PGModularAppearanceMeshComponent.generated.h"

/** The leader can contain only a face; clothing must still enclose the animated body. */
UCLASS()
class PGACTOR_API UPGModularAppearanceMeshComponent : public USkeletalMeshComponent
{
    GENERATED_BODY()
public:
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};

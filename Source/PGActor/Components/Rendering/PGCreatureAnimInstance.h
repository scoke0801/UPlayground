#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PGCreatureAnimInstance.generated.h"

/** Small skeleton-independent locomotion graph for imported creatures. */
UCLASS(Transient, NotBlueprintable)
class PGACTOR_API UPGCreatureAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

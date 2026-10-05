#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PGAppearanceAnimInstance.generated.h"

/** Evaluates a cosmetic retarget pose after the gameplay mesh has finished evaluating. */
UCLASS(Transient)
class PGACTOR_API UPGAppearanceAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<class UIKRetargeter> Retargeter;
    UPROPERTY(Transient) bool bReconstructScaledTranslations = false;
    UPROPERTY(Transient) TObjectPtr<class UPGCharacterAppearance> Appearance;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

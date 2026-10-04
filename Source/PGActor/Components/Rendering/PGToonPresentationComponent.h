#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGToonPresentationComponent.generated.h"

class ADirectionalLight;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

/** Synchronizes toon art controls with a chosen world light and the animated head.
 * Actual illumination/shadow reception stays in the engine's Default Lit renderer.
 */
UCLASS(ClassGroup=(PG), meta=(BlueprintSpawnableComponent))
class PGACTOR_API UPGToonPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPGToonPresentationComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    TObjectPtr<ADirectionalLight> KeyLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FName HeadBone = NAME_None;
    // Axes in the head bone's local reference frame (calibrated once per skeleton).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FVector HeadForwardAxis = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FVector HeadRightAxis = FVector::RightVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FVector FallbackLightDirection = FVector(.35, -.45, -.82);

    UFUNCTION(BlueprintCallable, Category="PG|Toon")
    void Initialize(USkeletalMeshComponent* InMesh);
    UFUNCTION(BlueprintCallable, Category="PG|Toon")
    void RefreshPresentation();
    UFUNCTION(BlueprintPure, Category="PG|Toon")
    int32 GetDynamicMaterialCount() const { return Materials.Num(); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> HeadMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInterface>> OriginalMaterials;
    FVector LastLight = FVector::ZeroVector;
    FVector LastForward = FVector::ZeroVector;
    FVector LastRight = FVector::ZeroVector;
    void RestoreMaterials();
};

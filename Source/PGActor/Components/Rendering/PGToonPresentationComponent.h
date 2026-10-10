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
    // A level can tag its sun explicitly. Untagged legacy levels use a stable,
    // brightest visible directional light rather than actor enumeration order.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FName KeyLightTag = TEXT("PGToonKeyLight");
    static ADirectionalLight* ResolveKeyLight(UWorld* World, FName Tag = TEXT("PGToonKeyLight"));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FName HeadBone = NAME_None;
    // Axes in the head bone's local reference frame (calibrated once per skeleton).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FVector HeadForwardAxis = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FVector HeadRightAxis = FVector::RightVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon")
    FVector FallbackLightDirection = FVector(.35, -.45, -.82);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon|Quality") TObjectPtr<class UStaticMesh> HairShadowMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon|Quality") bool bHairShadowEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon|Quality", meta=(ClampMin="0")) float HairShadowDistance = 700.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon|Quality", meta=(ClampMin="0")) float DetailDistance = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon|Quality", meta=(ClampMin="1")) float SimpleDistance = 1600.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Toon|Quality", meta=(ClampMin="0", ClampMax="1")) float FarDetailWeight = .35f;
    UFUNCTION(BlueprintPure, Category="PG|Toon|Quality") class UStaticMeshComponent* GetHairShadowProxy() const { return HairProxy; }
    static float CalculateDetailWeight(float Distance, float Near, float Far, float Minimum);

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
    UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> HairProxy;
    FVector LastLight = FVector::ZeroVector;
    FVector LastForward = FVector::ZeroVector;
    FVector LastRight = FVector::ZeroVector;
    float LightRetrySeconds = 0.f;
    float QualitySeconds = 0.f;
    float LastDetailWeight = -1.f;
    float HeadUpdateSeconds = 0.f;
    bool bDistant = false;
    void RefreshQuality(float DeltaTime);
    void RestoreMaterials();
};

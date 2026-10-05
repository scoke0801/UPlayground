#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGPlayerDashComponent.generated.h"

USTRUCT()
struct FPGDashGhost
{
    GENERATED_BODY()
    UPROPERTY() TArray<TObjectPtr<class UPoseableMeshComponent>> Meshes;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Material;
    float Age = 100.f;
};

// Bounded, reusable pose snapshots. No tick outside the dash/fade window.
UCLASS(ClassGroup=(PG), meta=(BlueprintSpawnableComponent))
class PGACTOR_API UPGPlayerDashComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPGPlayerDashComponent();
    UPROPERTY(EditAnywhere, Category="PG|Dash", meta=(ClampMin="50", ClampMax="1000")) float Distance = 450.f;
    UPROPERTY(EditAnywhere, Category="PG|Dash", meta=(ClampMin="0.15", ClampMax="1")) float Duration = .36f;
    UPROPERTY(EditAnywhere, Category="PG|Dash") TSoftObjectPtr<class UMaterialInterface> AfterimageMaterial;
    UPROPERTY(EditAnywhere, Category="PG|Dash") FLinearColor Tint = FLinearColor(.12f, .7f, 1.f);
    UPROPERTY(EditAnywhere, Category="PG|Dash", meta=(ClampMin="0.02", ClampMax="0.2")) float SnapshotInterval = .045f;
    UPROPERTY(EditAnywhere, Category="PG|Dash", meta=(ClampMin="0.05", ClampMax="0.5")) float FadeSeconds = .24f;
    void Start();
    void PrepareAfterimages();
    void Stop(bool bClear = false);
    bool IsDashing() const { return bDashing; }
    int32 GetVisibleGhostCount() const;
    virtual void TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Function) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TArray<FPGDashGhost> Ghosts;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInterface> LoadedMaterial;
    TWeakObjectPtr<class UCapsuleComponent> DashCollisionCapsule;
    ECollisionResponse SavedEnemyResponse = ECR_Block;
    bool bDashing = false;
    float SinceSnapshot = 0.f;
    int32 NextGhost = 0;
    FVector LastSnapshotLocation = FVector::ZeroVector;
    void Capture();
};

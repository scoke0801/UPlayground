#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGLootDrop.generated.h"
UCLASS()
class PGACTOR_API APGLootDrop : public AActor
{
    GENERATED_BODY()
public:
    APGLootDrop();
    void InitializeItem(const FPGItemInstance& InItem);
    bool TryPickup(APawn* Player);
    const FPGItemInstance& GetItem() const { return Item; }
    static void SpawnForEnemy(AActor* Enemy);
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<class UWidgetComponent> Label;
    UPROPERTY() TObjectPtr<class UNiagaraComponent> Beam;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> BeamMesh;
    float ArcElapsed = 0.f;
    float ArcDuration = .45f;
    float ArcHeight = 100.f;
    UPROPERTY() FPGItemInstance Item;
    bool bClaimed = false;
};

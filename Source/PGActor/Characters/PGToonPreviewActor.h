#pragma once

#include "CoreMinimal.h"
#include "Animation/SkeletalMeshActor.h"
#include "PGToonPreviewActor.generated.h"

/** Isolated lighting/animation fixture. Does not replace the gameplay player. */
UCLASS()
class PGACTOR_API APGToonPreviewActor : public ASkeletalMeshActor
{
    GENERATED_BODY()
public:
    APGToonPreviewActor();
    /** Runtime art-fixture spawning for Python/Blueprint validation (no editor-world fallback). */
    UFUNCTION(BlueprintCallable, Category="PG|Toon|Test", meta=(WorldContext="WorldContextObject"))
    static APGToonPreviewActor* SpawnPreviewActor(UObject* WorldContextObject, const FTransform& Transform);
    /** Fix the fixture render target size independently of editor panel layout; 0/0 restores it. */
    UFUNCTION(BlueprintCallable, Category="PG|Toon|Test", meta=(WorldContext="WorldContextObject"))
    static bool SetPreviewViewportSize(UObject* WorldContextObject, int32 Width, int32 Height);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PG|Toon")
    TObjectPtr<class UPGToonPresentationComponent> ToonPresentation;
    /** Optional masked hair caster; shares the visible mesh's pose, never renders color/depth. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PG|Toon")
    TObjectPtr<class USkeletalMeshComponent> HairShadowProxy;

protected:
    virtual void BeginPlay() override;
};

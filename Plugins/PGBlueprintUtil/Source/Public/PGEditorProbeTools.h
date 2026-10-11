#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGEditorProbeTools.generated.h"

/** Editor-only helpers for isolated, rendered combat probes. Never saves editor settings. */
UCLASS()
class PGBLUEPRINTUTIL_API UPGEditorProbeTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Temporary fixture actor, restricted to PIE worlds; never edits a level asset. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static AActor* SpawnPlayActor(UWorld* World, TSubclassOf<AActor> ActorClass, FTransform Transform);
    /** Compose project-owned clips; recalculates section links and duration after replacing segments. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool ConfigureMotionMontage(class UAnimMontage* Montage, const TArray<class UAnimSequence*>& Clips);
    /** Creates navigation data for a newly authored editor world; never saves it. */
    UFUNCTION(BlueprintCallable, Category="PG|Editor")
    static class AActor* EnsureEditorNavigation(UWorld* World, class ANavMeshBoundsVolume* Bounds);

    /** Returns zero unless a complete path exists in the current PIE world. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static int32 GetPlayNavigationPathPointCount(UWorld* World, FVector Start, FVector End, AActor* Agent);

    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool BeginPlayWindow(int32 Width = 1280, int32 Height = 720);

    /** The PIE world destroys the camera when play ends; no editor actor is created. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static class ACameraActor* SpawnPlayCamera(UWorld* World, FVector Location, FRotator Rotation);

    /** Captures this PIE world's viewport and HUD; rejects an empty render. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool CaptureGameViewport(UWorld* World, const FString& Filename);

    /** Resize only the active PIE viewport; does not save user display settings. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool ResizePlayViewport(UWorld* World, int32 Width, int32 Height);

    /** Read imported LOD0 positions/normals/UV0, bypassing FBX material baking. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool ExportSkeletalMaterialGeometry(class USkeletalMesh* Mesh, FName MaterialSlot, const FString& Filename);
};

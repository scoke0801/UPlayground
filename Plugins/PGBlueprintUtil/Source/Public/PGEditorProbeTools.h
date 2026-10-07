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
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool BeginPlayWindow(int32 Width = 1280, int32 Height = 720);

    /** The PIE world destroys the camera when play ends; no editor actor is created. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static class ACameraActor* SpawnPlayCamera(UWorld* World, FVector Location, FRotator Rotation);

    /** Captures this PIE world's viewport and HUD; rejects an empty render. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool CaptureGameViewport(UWorld* World, const FString& Filename);

    /** Read imported LOD0 positions/normals/UV0, bypassing FBX material baking. */
    UFUNCTION(BlueprintCallable, Category="PG|QA")
    static bool ExportSkeletalMaterialGeometry(class USkeletalMesh* Mesh, FName MaterialSlot, const FString& Filename);
};

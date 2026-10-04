#include "PGToonPreviewActor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Slate/SceneViewport.h"
#include "../Components/Rendering/PGToonPresentationComponent.h"

APGToonPreviewActor::APGToonPreviewActor()
{
    ToonPresentation = CreateDefaultSubobject<UPGToonPresentationComponent>(TEXT("ToonPresentation"));
}

APGToonPreviewActor* APGToonPreviewActor::SpawnPreviewActor(UObject* WorldContextObject, const FTransform& Transform)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || !World->IsGameWorld())
        return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<APGToonPreviewActor>(StaticClass(), Transform, Params);
}

bool APGToonPreviewActor::SetPreviewViewportSize(UObject* WorldContextObject, int32 Width, int32 Height)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || !World->IsGameWorld() || !World->GetGameViewport())
        return false;
    FSceneViewport* Viewport = World->GetGameViewport()->GetGameViewport();
    if (!Viewport || !((Width == 0 && Height == 0) || (Width >= 64 && Height >= 64 && Width <= 4096 && Height <= 4096)))
        return false;
    Viewport->SetFixedViewportSize(Width, Height);
    return true;
}

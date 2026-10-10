#include "PGEditorProbeTools.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Slate/SceneViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "AI/NavigationSystemBase.h"
#include "GameFramework/WorldSettings.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"

AActor* UPGEditorProbeTools::EnsureEditorNavigation(UWorld* World, ANavMeshBoundsVolume* Bounds)
{
    if (!GEditor || !IsValid(World) || World->WorldType != EWorldType::Editor ||
        !IsValid(Bounds) || Bounds->GetWorld() != World) return nullptr;
    if (!World->GetNavigationSystem())
        FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode,
            World->GetWorldSettings()->GetNavigationSystemConfig(), true);
    auto* Navigation = UNavigationSystemV1::GetCurrent(World);
    if (!Navigation) return nullptr;
    Navigation->OnNavigationBoundsUpdated(Bounds);
    return Navigation->GetDefaultNavDataInstance(FNavigationSystem::Create);
}

int32 UPGEditorProbeTools::GetPlayNavigationPathPointCount(UWorld* World, FVector Start, FVector End, AActor* Agent)
{
    if (!GEditor || !IsValid(World) || GEditor->PlayWorld != World ||
        !IsValid(Agent) || Agent->GetWorld() != World) return 0;
    // Native invocation avoids calling ProcessEvent on NavigationSystem's CDO.
    const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, End, Agent);
    return IsValid(Path) && Path->IsValid() && !Path->IsPartial() ? Path->PathPoints.Num() : 0;
}

bool UPGEditorProbeTools::ExportSkeletalMaterialGeometry(USkeletalMesh* Mesh, FName MaterialSlot, const FString& Filename)
{
    if (!IsValid(Mesh) || Filename.IsEmpty()) return false;
    const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
    if (!Model || Model->LODModels.IsEmpty()) return false;
    const int32 MaterialIndex = Mesh->GetMaterials().IndexOfByPredicate([MaterialSlot](const FSkeletalMaterial& Material)
    {
        return Material.MaterialSlotName == MaterialSlot;
    });
    if (MaterialIndex == INDEX_NONE) return false;
    const FSkeletalMeshLODModel& LOD = Model->LODModels[0];
    TArray<TSharedPtr<FJsonValue>> Vertices, Triangles;
    for (const FSkelMeshSection& Section : LOD.Sections)
    {
        if (Section.MaterialIndex != MaterialIndex) continue;
        const int32 Offset = Vertices.Num();
        for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            const float Data[] = { Vertex.Position.X, Vertex.Position.Y, Vertex.Position.Z,
                Vertex.TangentZ.X, Vertex.TangentZ.Y, Vertex.TangentZ.Z, Vertex.UVs[0].X, Vertex.UVs[0].Y };
            for (const float Value : Data) Values.Add(MakeShared<FJsonValueNumber>(Value));
            Vertices.Add(MakeShared<FJsonValueArray>(Values));
        }
        for (uint32 Triangle = 0; Triangle < Section.NumTriangles; ++Triangle)
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            for (uint32 Corner = 0; Corner < 3; ++Corner)
            {
                const uint32 Index = LOD.IndexBuffer[Section.BaseIndex + Triangle * 3 + Corner] - Section.BaseVertexIndex;
                if (!Section.SoftVertices.IsValidIndex(Index)) return false;
                Values.Add(MakeShared<FJsonValueNumber>(Offset + Index));
            }
            Triangles.Add(MakeShared<FJsonValueArray>(Values));
        }
    }
    if (Vertices.IsEmpty() || Triangles.IsEmpty()) return false;
    const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Object->SetStringField(TEXT("slot"), MaterialSlot.ToString());
    Object->SetNumberField(TEXT("lod"), 0);
    Object->SetArrayField(TEXT("vertices"), Vertices);
    Object->SetArrayField(TEXT("triangles"), Triangles);
    FString Payload;
    if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Payload))) return false;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    return FFileHelper::SaveStringToFile(Payload, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UPGEditorProbeTools::BeginPlayWindow(int32 Width, int32 Height)
{
    if (!GEditor || GEditor->PlayWorld || !FSlateApplication::IsInitialized() || Width < 320 || Height < 240) return false;
    auto* Settings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
    Settings->NewWindowWidth = Width;
    Settings->NewWindowHeight = Height;
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings = Settings;
    // Default PIE windows put a Slate title bar inside the requested client size.
    // An OS border leaves the entire client area available to the game viewport.
    const auto Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("PG 전투 검증")))
        .ClientSize(FVector2D(Width, Height))
        .UseOSWindowBorder(true)
        .AutoCenter(EAutoCenter::PreferredWorkArea)
        .SizingRule(ESizingRule::FixedSize)
        .AdjustInitialSizeAndPositionForDPIScale(false);
    FSlateApplication::Get().AddWindow(Window);
    Params.CustomPIEWindow = Window;
    GEditor->RequestPlaySession(Params);
    return true;
}

ACameraActor* UPGEditorProbeTools::SpawnPlayCamera(UWorld* World, FVector Location, FRotator Rotation)
{
    if (!World || !World->IsGameWorld()) return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<ACameraActor>(Location, Rotation, Params);
}

bool UPGEditorProbeTools::ResizePlayViewport(UWorld* World, int32 Width, int32 Height)
{
    if (!World || World->WorldType != EWorldType::PIE || Width < 320 || Height < 240 || Width > 3840 || Height > 2160) return false;
    auto* Client = World->GetGameViewport();
    const auto Widget = Client ? Client->GetGameViewportWidget() : nullptr;
    if (!Widget || !Client->GetGameViewport() || !FSlateApplication::IsInitialized()) return false;
    const auto Window = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef());
    if (!Window) return false;
    Window->Resize(FVector2D(Width, Height));
    Client->GetGameViewport()->SetFixedViewportSize(Width, Height);
    return true;
}

bool UPGEditorProbeTools::CaptureGameViewport(UWorld* World, const FString& Filename)
{
    auto* Client = World ? World->GetGameViewport() : nullptr;
    const auto Widget = Client ? Client->GetGameViewportWidget() : nullptr;
    if (!Widget || !FSlateApplication::IsInitialized() || Filename.IsEmpty()) return false;
    TArray<FColor> Pixels;
    FIntVector Size;
    if (!FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size) || Pixels.IsEmpty()) return false;
    bool bVisible = false;
    for (auto& Pixel : Pixels)
    {
        Pixel.A = 255;
        bVisible |= Pixel.R > 8 || Pixel.G > 8 || Pixel.B > 8;
    }
    if (!bVisible) return false;
    TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    return FFileHelper::SaveArrayToFile(PNG, *Filename);
}

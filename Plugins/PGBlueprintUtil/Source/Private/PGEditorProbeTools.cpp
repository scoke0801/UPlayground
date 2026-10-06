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
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

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

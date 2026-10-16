#pragma once

#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Offline, fixed-step visual evidence. Readback cost is not a performance measurement.
struct FPGCombatFilm
{
    bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("PGCombatFilm"));
    int32 Frame = 0;
    void Capture()
    {
        if (!bEnabled) return;
        FApp::SetFixedDeltaTime(1. / 60.);
        FApp::SetUseFixedTimeStep(true);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/CombatFilm") /
            FString::Printf(TEXT("Frame%06d.png"), Frame++), false, false);
    }
};

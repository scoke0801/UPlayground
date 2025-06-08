#include "PGBlueprintUtil.h"

DEFINE_LOG_CATEGORY(LogPGBlueprintUtil);

#define LOCTEXT_NAMESPACE "FPGBlueprintUtilModule"

void FPGBlueprintUtilModule::StartupModule()
{
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("PGBlueprintUtil: StartupModule"));
}

void FPGBlueprintUtilModule::ShutdownModule()
{
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("PGBlueprintUtil: ShutdownModule"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPGBlueprintUtilModule, PGBlueprintUtil)

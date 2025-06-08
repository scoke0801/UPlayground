#include "PGDataModule.h"

DEFINE_LOG_CATEGORY(LogPGDataModule);

#define LOCTEXT_NAMESPACE "FPGDataModule"

void FPGDataModule::StartupModule()
{
	UE_LOG(LogPGDataModule, Log, TEXT("PGDataModule: StartupModule"));
}

void FPGDataModule::ShutdownModule()
{
	UE_LOG(LogPGDataModule, Log, TEXT("PGDataModule: ShutdownModule"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPGDataModule, PGDataModule)

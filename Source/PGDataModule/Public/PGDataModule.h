#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPGDataModule, Log, All);

/**
 * PGDataModule
 * 
 * 데이터 관리를 위한 모듈입니다.
 */
class PGDATAMODULE_API FPGDataModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/**
	 * 모듈이 로드되었는지 확인합니다.
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("PGDataModule");
	}
	
	/**
	 * 모듈 인스턴스를 가져옵니다.
	 */
	static inline FPGDataModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FPGDataModule>("PGDataModule");
	}
};

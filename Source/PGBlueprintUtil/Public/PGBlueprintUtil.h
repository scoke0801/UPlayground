#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPGBlueprintUtil, Log, All);

/**
 * PGBlueprintUtil
 * 
 * 블루프린트 유틸리티를 위한 모듈입니다.
 */
class PGBLUEPRINTUTIL_API FPGBlueprintUtilModule : public IModuleInterface
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
		return FModuleManager::Get().IsModuleLoaded("PGBlueprintUtil");
	}
	
	/**
	 * 모듈 인스턴스를 가져옵니다.
	 */
	static inline FPGBlueprintUtilModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FPGBlueprintUtilModule>("PGBlueprintUtil");
	}
};

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogPGBlueprintUtil, Log, All);

/**
 * PGBlueprintUtil
 * 
 * 블루프린트 유틸리티를 위한 모듈입니다.
 * 런타임과 에디터 기능을 모두 제공합니다.
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

#if WITH_EDITOR
	/**
	 * 블루프린트 검색 창을 엽니다.
	 */
	void OpenBlueprintSearchWindow();

private:
	/** 에디터 초기화 */
	void OnLevelEditorCreated(TSharedPtr<class ILevelEditor> LevelEditor);

	/** 탭 스포너 등록 */
	void RegisterTabSpawners();

	/** 메뉴 확장 등록 */
	void RegisterMenuExtensions();

	/** 메뉴 등록 */
	void RegisterMenus();

	/** 블루프린트 검색 탭 생성 */
	TSharedRef<SDockTab> OnSpawnBlueprintSearchTab(const FSpawnTabArgs& Args);
#endif // WITH_EDITOR
};

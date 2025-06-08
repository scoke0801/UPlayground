#include "PGBlueprintUtilCommands.h"

#if WITH_EDITOR
#include "PGBlueprintUtil.h"
#include "LevelEditor.h"
#include "Framework/Commands/UICommandList.h"

#define LOCTEXT_NAMESPACE "PGBlueprintUtilCommands"

void FPGBlueprintUtilCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenBlueprintSearch,
		"부모 클래스별 블루프린트 검색",
		"지정된 부모 클래스를 상속받는 모든 블루프린트를 검색합니다",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::B, EModifierKey::Control | EModifierKey::Shift)
	);

	// 명령어 액션 바인딩
	FUIAction OpenSearchAction(
		FExecuteAction::CreateLambda([]()
		{
			if (FPGBlueprintUtilModule::IsAvailable())
			{
				FPGBlueprintUtilModule::Get().OpenBlueprintSearchWindow();
			}
		}),
		FCanExecuteAction::CreateLambda([]()
		{
			return true; // 항상 실행 가능
		})
	);

	// 전역 명령어 맵에 등록
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		TSharedPtr<FUICommandList> CommandList = LevelEditorModule->GetGlobalLevelEditorActions();
		if (CommandList.IsValid())
		{
			CommandList->MapAction(OpenBlueprintSearch, OpenSearchAction);
		}
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR

#include "PGBlueprintUtil.h"

#if WITH_EDITOR
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "PGBlueprintUtilCommands.h"
#include "PGBlueprintSearchWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Application/SlateApplication.h"
#endif

DEFINE_LOG_CATEGORY(LogPGBlueprintUtil);

#define LOCTEXT_NAMESPACE "FPGBlueprintUtilModule"

#if WITH_EDITOR
static const FName PGBlueprintSearchTabName("PGBlueprintSearch");
#endif

void FPGBlueprintUtilModule::StartupModule()
{
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("PGBlueprintUtil: StartupModule"));

#if WITH_EDITOR
	// 에디터가 준비되면 메뉴 등록
	if (!IsRunningCommandlet())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.OnLevelEditorCreated().AddRaw(this, &FPGBlueprintUtilModule::OnLevelEditorCreated);
	}
#endif
}

void FPGBlueprintUtilModule::ShutdownModule()
{
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("PGBlueprintUtil: ShutdownModule"));

#if WITH_EDITOR
	// 명령어 등록 해제
	FPGBlueprintUtilCommands::Unregister();

	// 탭 등록 해제
	if (FGlobalTabmanager::Get()->HasTabSpawner(PGBlueprintSearchTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PGBlueprintSearchTabName);
	}

	// 메뉴 확장 해제
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
#endif
}

#if WITH_EDITOR

void FPGBlueprintUtilModule::OnLevelEditorCreated(TSharedPtr<class ILevelEditor> LevelEditor)
{
	// 명령어 등록
	FPGBlueprintUtilCommands::Register();

	// 탭 스포너 등록
	RegisterTabSpawners();

	// 메뉴 확장 등록
	RegisterMenuExtensions();
}

void FPGBlueprintUtilModule::RegisterTabSpawners()
{
	auto RegisterTabSpawner = [this]()
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			PGBlueprintSearchTabName,
			FOnSpawnTab::CreateRaw(this, &FPGBlueprintUtilModule::OnSpawnBlueprintSearchTab)
		)
		.SetDisplayName(LOCTEXT("BlueprintSearchTabTitle", "블루프린트 검색"))
		.SetTooltipText(LOCTEXT("BlueprintSearchTabTooltip", "부모 클래스별 블루프린트 검색 도구"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.FindInBlueprint"));
	};

	RegisterTabSpawner();
}

void FPGBlueprintUtilModule::RegisterMenuExtensions()
{
	// ToolMenus 시스템을 통한 메뉴 확장
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPGBlueprintUtilModule::RegisterMenus)
	);
}

void FPGBlueprintUtilModule::RegisterMenus()
{
	// 메인 메뉴바의 블루프린트 메뉴 확장
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* BlueprintMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Blueprint");
		if (BlueprintMenu)
		{
			FToolMenuSection& Section = BlueprintMenu->FindOrAddSection("BlueprintUtilities");
			Section.Label = LOCTEXT("BlueprintUtilitiesSection", "블루프린트 유틸리티");

			Section.AddMenuEntry(
				FPGBlueprintUtilCommands::Get().OpenBlueprintSearch,
				TAttribute<FText>(),
				TAttribute<FText>(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.FindInBlueprint")
			);
		}
	}

	// 툴바에도 추가 (선택적)
	{
		UToolMenu* ToolBar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		if (ToolBar)
		{
			FToolMenuSection& Section = ToolBar->FindOrAddSection("BlueprintUtils");
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				FPGBlueprintUtilCommands::Get().OpenBlueprintSearch,
				LOCTEXT("BlueprintSearchLabel", "BP 검색"),
				LOCTEXT("BlueprintSearchTooltip", "부모 클래스별 블루프린트 검색"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.FindInBlueprint")
			));
		}
	}
}

TSharedRef<SDockTab> FPGBlueprintUtilModule::OnSpawnBlueprintSearchTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SPGBlueprintSearchWidget)
		];
}

void FPGBlueprintUtilModule::OpenBlueprintSearchWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PGBlueprintSearchTabName);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPGBlueprintUtilModule, PGBlueprintUtil)

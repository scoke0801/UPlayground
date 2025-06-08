#include "PGBlueprintSearchWidget.h"

#if WITH_EDITOR
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "ClassViewerModule.h"
#include "Styling/SlateTypes.h"
#include "Styling/AppStyle.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "AssetRegistry/AssetData.h"
#include "PGBlueprintUtil.h"

#define LOCTEXT_NAMESPACE "PGBlueprintSearchWidget"

// 컬럼 이름 정의
const FName SPGBlueprintSearchWidget::ColumnName_Name = "Name";
const FName SPGBlueprintSearchWidget::ColumnName_ParentClass = "ParentClass";  
const FName SPGBlueprintSearchWidget::ColumnName_Path = "Path";
const FName SPGBlueprintSearchWidget::ColumnName_Size = "Size";
const FName SPGBlueprintSearchWidget::ColumnName_LastModified = "LastModified";

void SPGBlueprintSearchWidget::Construct(const FArguments& InArgs)
{
	// 검색 도구 초기화
	SearchTool = NewObject<UPGBlueprintSearchTool>();
	SearchTool->OnSearchCompleted.AddSP(this, &SPGBlueprintSearchWidget::OnSearchCompleted);
	SearchTool->OnSearchProgressUpdated.AddSP(this, &SPGBlueprintSearchWidget::OnSearchProgressUpdated);

	// 검색 범위 옵션 초기화
	SearchScopeOptions.Add(MakeShareable(new EPGSearchScope(EPGSearchScope::ProjectOnly)));
	SearchScopeOptions.Add(MakeShareable(new EPGSearchScope(EPGSearchScope::IncludePlugins)));
	SearchScopeOptions.Add(MakeShareable(new EPGSearchScope(EPGSearchScope::IncludeEngine)));
	CurrentCriteria.SearchScope = EPGSearchScope::ProjectOnly;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(4)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			// 메인 검색 영역
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SVerticalBox)
				
				// 제목
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WindowTitle", "부모 클래스별 블루프린트 검색"))
					.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
				]

				// 클래스 선택 영역
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					CreateClassSelectionArea()
				]

				// 검색 옵션 영역
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					CreateSearchOptionsArea()
				]

				// 고급 옵션 영역
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					CreateAdvancedOptionsArea()
				]

				// 결과 영역
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(4)
				[
					CreateResultsArea()
				]

				// 상태 영역
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					CreateStatusArea()
				]
			]

			// 사이드 패널 (히스토리, 북마크, 내보내기)
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				CreateSidePanel()
			]
		]
	];
}

SPGBlueprintSearchWidget::~SPGBlueprintSearchWidget()
{
	if (SearchTool)
	{
		SearchTool->CancelSearch();
	}
}

TSharedRef<SWidget> SPGBlueprintSearchWidget::CreateClassSelectionArea()
{
	// 클래스 뷰어 초기화 옵션
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	
	// 커스텀 필터 설정
	TSharedPtr<FPGBlueprintParentClassFilter> ClassFilter = MakeShareable(new FPGBlueprintParentClassFilter());
	Options.ClassFilters.Add(ClassFilter.ToSharedRef());

	// 클래스 뷰어 생성
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectParentClass", "부모 클래스 선택:"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		[
			SNew(SBox)
			.HeightOverride(150)
			[
				ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SPGBlueprintSearchWidget::OnClassSelected))
			]
		];
}

TSharedRef<SWidget> SPGBlueprintSearchWidget::CreateSearchOptionsArea()
{
	return SNew(SHorizontalBox)
		
		// 검색 범위
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SearchScope", "검색 범위:"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ScopeComboBox, SComboBox<TSharedPtr<EPGSearchScope>>)
				.OptionsSource(&SearchScopeOptions)
				.OnGenerateWidget(this, &SPGBlueprintSearchWidget::OnGenerateScopeWidget)
				.OnSelectionChanged(this, &SPGBlueprintSearchWidget::OnScopeChanged)
				.InitiallySelectedItem(SearchScopeOptions[0])
				[
					SNew(STextBlock)
					.Text(this, &SPGBlueprintSearchWidget::GetCurrentScopeText)
				]
			]
		]

		// 옵션 체크박스들
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SCheckBox)
				.IsChecked(CurrentCriteria.bIncludeAbstractClasses ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					CurrentCriteria.bIncludeAbstractClasses = (NewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("IncludeAbstract", "추상 클래스 포함"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SCheckBox)
				.IsChecked(CurrentCriteria.bExactMatchOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					CurrentCriteria.bExactMatchOnly = (NewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ExactMatchOnly", "정확한 일치만"))
				]
			]
		]

		// 검색 버튼들
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.Text(this, &SPGBlueprintSearchWidget::GetSearchButtonText)
				.OnClicked(this, &SPGBlueprintSearchWidget::OnSearchClicked)
				.IsEnabled(this, &SPGBlueprintSearchWidget::IsSearchButtonEnabled)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.Text(LOCTEXT("Clear", "지우기"))
				.OnClicked(this, &SPGBlueprintSearchWidget::OnClearClicked)
			]
		]

		// 북마크 추가 버튼
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddBookmark", "북마크 추가"))
			.OnClicked(this, &SPGBlueprintSearchWidget::OnAddBookmarkClicked)
			.IsEnabled(this, &SPGBlueprintSearchWidget::IsSearchButtonEnabled)
		];
}

TSharedRef<SWidget> SPGBlueprintSearchWidget::CreateAdvancedOptionsArea()
{
	return SAssignNew(AdvancedFiltersArea, SExpandableArea)
		.AreaTitle(LOCTEXT("AdvancedFilters", "고급 필터"))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SAssignNew(AdvancedFilterWidget, SPGAdvancedFilterWidget)
			.AdvancedFilter(&CurrentCriteria.AdvancedFilter)
		];
}

TSharedRef<SWidget> SPGBlueprintSearchWidget::CreateResultsArea()
{
	return SNew(SVerticalBox)
		
		// 필터 검색박스
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SAssignNew(FilterSearchBox, SSearchBox)
			.HintText(LOCTEXT("FilterHint", "결과 필터링..."))
			.OnTextChanged(this, &SPGBlueprintSearchWidget::OnFilterTextChanged)
		]

		// 결과 리스트
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(ResultListView, SListView<TSharedPtr<FPGBlueprintSearchResult>>)
			.ListItemsSource(&FilteredResults)
			.OnGenerateRow(this, &SPGBlueprintSearchWidget::OnGenerateResultRow)
			.OnMouseButtonDoubleClick(this, &SPGBlueprintSearchWidget::OnResultDoubleClicked)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ColumnName_Name)
				.DefaultLabel(LOCTEXT("NameColumn", "이름"))
				.FillWidth(0.25f)

				+ SHeaderRow::Column(ColumnName_ParentClass)
				.DefaultLabel(LOCTEXT("ParentClassColumn", "부모 클래스"))
				.FillWidth(0.2f)

				+ SHeaderRow::Column(ColumnName_Path)
				.DefaultLabel(LOCTEXT("PathColumn", "경로"))
				.FillWidth(0.35f)

				+ SHeaderRow::Column(ColumnName_Size)
				.DefaultLabel(LOCTEXT("SizeColumn", "크기"))
				.FillWidth(0.1f)

				+ SHeaderRow::Column(ColumnName_LastModified)
				.DefaultLabel(LOCTEXT("LastModifiedColumn", "수정일"))
				.FillWidth(0.1f)
			)
		];
}

TSharedRef<SWidget> SPGBlueprintSearchWidget::CreateStatusArea()
{
	return SNew(SHorizontalBox)
		
		// 상태 텍스트
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(this, &SPGBlueprintSearchWidget::GetStatusText)
		]

		// 진행률 바
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8, 0, 0, 0)
		[
			SNew(SProgressBar)
			.Visibility(this, &SPGBlueprintSearchWidget::GetProgressBarVisibility)
			.Percent(this, &SPGBlueprintSearchWidget::GetProgressPercent)
		];
}

TSharedRef<SWidget> SPGBlueprintSearchWidget::CreateSidePanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4)
		[
			SNew(SVerticalBox)

			// 검색 히스토리
			+ SVerticalBox::Slot()
			.FillHeight(0.4f)
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(HistoryArea, SExpandableArea)
				.AreaTitle(LOCTEXT("SearchHistory", "검색 히스토리"))
				.InitiallyCollapsed(false)
				.BodyContent()
				[
					SAssignNew(HistoryWidget, SPGSearchHistoryWidget)
					.SearchTool(SearchTool)
				]
			]

			// 북마크
			+ SVerticalBox::Slot()
			.FillHeight(0.4f)
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(BookmarksArea, SExpandableArea)
				.AreaTitle(LOCTEXT("Bookmarks", "북마크"))
				.InitiallyCollapsed(false)
				.BodyContent()
				[
					SAssignNew(BookmarkWidget, SPGBookmarkWidget)
					.SearchTool(SearchTool)
				]
			]

			// 내보내기
			+ SVerticalBox::Slot()
			.FillHeight(0.2f)
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("Export", "내보내기"))
				.InitiallyCollapsed(true)
				.BodyContent()
				[
					SAssignNew(ExportWidget, SPGExportWidget)
					.SearchResults(&FilteredResults)
				]
			]
		];
}

void SPGBlueprintSearchWidget::OnClassSelected(UClass* InClass)
{
	SelectedClass = InClass;
	CurrentCriteria.ParentClass = InClass;
	
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("Selected parent class: %s"), 
		   InClass ? *InClass->GetName() : TEXT("None"));
}

FReply SPGBlueprintSearchWidget::OnSearchClicked()
{
	if (!CurrentCriteria.IsValid())
	{
		UE_LOG(LogPGBlueprintUtil, Warning, TEXT("Cannot search: Invalid criteria"));
		return FReply::Handled();
	}

	if (bIsSearching)
	{
		// 검색 취소
		SearchTool->CancelSearch();
		bIsSearching = false;
	}
	else
	{
		// 검색 시작
		bIsSearching = true;
		SearchTool->SearchBlueprintsAsync(CurrentCriteria);
	}

	return FReply::Handled();
}

FReply SPGBlueprintSearchWidget::OnClearClicked()
{
	SearchResults.Empty();
	FilteredResults.Empty();
	ResultListView->RequestListRefresh();
	
	if (FilterSearchBox.IsValid())
	{
		FilterSearchBox->SetText(FText::GetEmpty());
	}
	
	CurrentProgress = 0.0f;
	
	return FReply::Handled();
}

FReply SPGBlueprintSearchWidget::OnAddBookmarkClicked()
{
	if (!CurrentCriteria.IsValid())
	{
		return FReply::Handled();
	}

	// 간단한 북마크 추가 (다이얼로그 없이)
	FPGBookmarkItem NewBookmark;
	NewBookmark.BookmarkName = FString::Printf(TEXT("검색_%s_%s"), 
		CurrentCriteria.ParentClass ? *CurrentCriteria.ParentClass->GetName() : TEXT("None"),
		*FDateTime::Now().ToString(TEXT("%m%d_%H%M")));
	NewBookmark.SearchCriteria = CurrentCriteria;
	NewBookmark.Description = FString::Printf(TEXT("%s 클래스를 상속받는 블루프린트 검색"), 
		CurrentCriteria.ParentClass ? *CurrentCriteria.ParentClass->GetName() : TEXT("None"));

	if (SearchTool->AddBookmark(NewBookmark))
	{
		if (BookmarkWidget.IsValid())
		{
			BookmarkWidget->RefreshBookmarks();
		}
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("Bookmark added: %s"), *NewBookmark.BookmarkName);
	}

	return FReply::Handled();
}

void SPGBlueprintSearchWidget::OnSearchCompleted(const TArray<FPGBlueprintSearchResult>& Results)
{
	bIsSearching = false;
	CurrentProgress = 1.0f;

	// 결과를 SharedPtr로 변환
	SearchResults.Empty();
	for (const FPGBlueprintSearchResult& Result : Results)
	{
		SearchResults.Add(MakeShareable(new FPGBlueprintSearchResult(Result)));
	}

	// 필터링된 결과 업데이트
	UpdateFilteredResults();

	// 히스토리와 북마크 위젯 새로 고침
	if (HistoryWidget.IsValid())
	{
		HistoryWidget->RefreshHistory();
	}

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("Search completed: %d results found"), Results.Num());
}

void SPGBlueprintSearchWidget::OnSearchProgressUpdated(float Progress)
{
	CurrentProgress = Progress;
}

void SPGBlueprintSearchWidget::OnFilterTextChanged(const FText& InFilterText)
{
	CurrentFilterText = InFilterText.ToString();
	UpdateFilteredResults();
}

void SPGBlueprintSearchWidget::OnResultDoubleClicked(TSharedPtr<FPGBlueprintSearchResult> Item)
{
	if (Item.IsValid())
	{
		Item->OpenBlueprint();
	}
}

TSharedPtr<SWidget> SPGBlueprintSearchWidget::OnResultRightClicked(TSharedPtr<FPGBlueprintSearchResult> Item)
{
	if (!Item.IsValid())
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenBlueprint", "블루프린트 열기"),
		LOCTEXT("OpenBlueprintTooltip", "블루프린트 에디터에서 엽니다"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([Item]()
		{
			Item->OpenBlueprint();
		}))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowInContentBrowser", "콘텐츠 브라우저에서 보기"),
		LOCTEXT("ShowInContentBrowserTooltip", "콘텐츠 브라우저에서 이 에셋을 선택합니다"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([Item]()
		{
			Item->ShowInContentBrowser();
		}))
	);

	return MenuBuilder.MakeWidget();
}

void SPGBlueprintSearchWidget::UpdateFilteredResults()
{
	FilteredResults.Empty();

	for (const auto& Result : SearchResults)
	{
		if (DoesResultMatchFilter(Result, CurrentFilterText))
		{
			FilteredResults.Add(Result);
		}
	}

	if (ResultListView.IsValid())
	{
		ResultListView->RequestListRefresh();
	}
}

bool SPGBlueprintSearchWidget::DoesResultMatchFilter(const TSharedPtr<FPGBlueprintSearchResult>& Result, const FString& FilterText) const
{
	if (FilterText.IsEmpty())
	{
		return true;
	}

	if (!Result.IsValid())
	{
		return false;
	}

	return Result->BlueprintName.Contains(FilterText) ||
		   Result->ParentClassName.Contains(FilterText) ||
		   Result->AssetPath.Contains(FilterText);
}

FText SPGBlueprintSearchWidget::GetSearchButtonText() const
{
	return bIsSearching ? LOCTEXT("Cancel", "취소") : LOCTEXT("Search", "검색");
}

bool SPGBlueprintSearchWidget::IsSearchButtonEnabled() const
{
	return CurrentCriteria.IsValid();
}

FText SPGBlueprintSearchWidget::GetStatusText() const
{
	if (bIsSearching)
	{
		return FText::Format(LOCTEXT("SearchingStatus", "검색 중... ({0}%)"), 
			FText::AsNumber(FMath::RoundToInt(CurrentProgress * 100)));
	}
	else if (FilteredResults.Num() > 0)
	{
		return FText::Format(LOCTEXT("ResultsStatus", "{0}개 결과 (총 {1}개 중)"), 
			FText::AsNumber(FilteredResults.Num()),
			FText::AsNumber(SearchResults.Num()));
	}
	else if (SearchResults.Num() > 0)
	{
		return LOCTEXT("NoFilteredResults", "필터 조건에 맞는 결과가 없습니다");
	}
	else
	{
		return LOCTEXT("ReadyStatus", "검색할 부모 클래스를 선택하고 검색 버튼을 누르세요");
	}
}

EVisibility SPGBlueprintSearchWidget::GetProgressBarVisibility() const
{
	return bIsSearching ? EVisibility::Visible : EVisibility::Hidden;
}

TOptional<float> SPGBlueprintSearchWidget::GetProgressPercent() const
{
	return CurrentProgress;
}

TSharedRef<SWidget> SPGBlueprintSearchWidget::OnGenerateScopeWidget(TSharedPtr<EPGSearchScope> InOption)
{
	FText ScopeText;
	if (InOption.IsValid())
	{
		switch (*InOption)
		{
		case EPGSearchScope::ProjectOnly:
			ScopeText = LOCTEXT("ProjectOnly", "프로젝트만");
			break;
		case EPGSearchScope::IncludePlugins:
			ScopeText = LOCTEXT("IncludePlugins", "플러그인 포함");
			break;
		case EPGSearchScope::IncludeEngine:
			ScopeText = LOCTEXT("IncludeEngine", "엔진 포함");
			break;
		}
	}

	return SNew(STextBlock).Text(ScopeText);
}

FText SPGBlueprintSearchWidget::GetCurrentScopeText() const
{
	switch (CurrentCriteria.SearchScope)
	{
	case EPGSearchScope::ProjectOnly:
		return LOCTEXT("ProjectOnly", "프로젝트만");
	case EPGSearchScope::IncludePlugins:
		return LOCTEXT("IncludePlugins", "플러그인 포함");
	case EPGSearchScope::IncludeEngine:
		return LOCTEXT("IncludeEngine", "엔진 포함");
	default:
		return LOCTEXT("Unknown", "알 수 없음");
	}
}

void SPGBlueprintSearchWidget::OnScopeChanged(TSharedPtr<EPGSearchScope> NewScope, ESelectInfo::Type SelectInfo)
{
	if (NewScope.IsValid())
	{
		CurrentCriteria.SearchScope = *NewScope;
	}
}

TSharedRef<ITableRow> SPGBlueprintSearchWidget::OnGenerateResultRow(TSharedPtr<FPGBlueprintSearchResult> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPGBlueprintSearchResultItem, OwnerTable)
		.SearchResult(Item);
}

// 클래스 필터 구현
bool FPGBlueprintParentClassFilter::IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs)
{
	// 블루프린트로 만들 수 있는 클래스만 허용
	return !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract) &&
		   (InClass->GetBoolMetaData(TEXT("BlueprintSpawnable")) || !InClass->HasAnyClassFlags(CLASS_NotPlaceable));
}

bool FPGBlueprintParentClassFilter::IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs)
{
	// 언로드된 블루프린트도 허용
	return true;
}

// 결과 아이템 위젯 구현
void SPGBlueprintSearchResultItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	SearchResult = InArgs._SearchResult;
	SMultiColumnTableRow<TSharedPtr<FPGBlueprintSearchResult>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SPGBlueprintSearchResultItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!SearchResult.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == SPGBlueprintSearchWidget::ColumnName_Name)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(SearchResult->BlueprintName));
	}
	else if (ColumnName == SPGBlueprintSearchWidget::ColumnName_ParentClass)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(SearchResult->ParentClassName));
	}
	else if (ColumnName == SPGBlueprintSearchWidget::ColumnName_Path)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(SearchResult->AssetPath));
	}
	else if (ColumnName == SPGBlueprintSearchWidget::ColumnName_Size)
	{
		return SNew(STextBlock)
			.Text(FText::AsMemory(SearchResult->FileSize));
	}
	else if (ColumnName == SPGBlueprintSearchWidget::ColumnName_LastModified)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(SearchResult->LastModified.ToString(TEXT("%m/%d %H:%M"))));
	}

	return SNullWidget::NullWidget;
}

// 고급 필터, 히스토리, 북마크, 내보내기 위젯 구현

// =============================================
// 고급 필터 위젯 구현
// =============================================

void SPGAdvancedFilterWidget::Construct(const FArguments& InArgs)
{
	AdvancedFilter = InArgs._AdvancedFilter;

	// 옵션 배열 초기화
	FileSizeOptions.Add(MakeShareable(new EPGFileSizeFilter(EPGFileSizeFilter::All)));
	FileSizeOptions.Add(MakeShareable(new EPGFileSizeFilter(EPGFileSizeFilter::Small)));
	FileSizeOptions.Add(MakeShareable(new EPGFileSizeFilter(EPGFileSizeFilter::Medium)));
	FileSizeOptions.Add(MakeShareable(new EPGFileSizeFilter(EPGFileSizeFilter::Large)));
	FileSizeOptions.Add(MakeShareable(new EPGFileSizeFilter(EPGFileSizeFilter::Custom)));

	DateFilterOptions.Add(MakeShareable(new EPGDateFilter(EPGDateFilter::All)));
	DateFilterOptions.Add(MakeShareable(new EPGDateFilter(EPGDateFilter::Today)));
	DateFilterOptions.Add(MakeShareable(new EPGDateFilter(EPGDateFilter::LastWeek)));
	DateFilterOptions.Add(MakeShareable(new EPGDateFilter(EPGDateFilter::LastMonth)));
	DateFilterOptions.Add(MakeShareable(new EPGDateFilter(EPGDateFilter::LastYear)));
	DateFilterOptions.Add(MakeShareable(new EPGDateFilter(EPGDateFilter::Custom)));

	SortCriteriaOptions.Add(MakeShareable(new EPGSortCriteria(EPGSortCriteria::Name)));
	SortCriteriaOptions.Add(MakeShareable(new EPGSortCriteria(EPGSortCriteria::ParentClass)));
	SortCriteriaOptions.Add(MakeShareable(new EPGSortCriteria(EPGSortCriteria::Path)));
	SortCriteriaOptions.Add(MakeShareable(new EPGSortCriteria(EPGSortCriteria::Size)));
	SortCriteriaOptions.Add(MakeShareable(new EPGSortCriteria(EPGSortCriteria::LastModified)));

	SortDirectionOptions.Add(MakeShareable(new EPGSortDirection(EPGSortDirection::Ascending)));
	SortDirectionOptions.Add(MakeShareable(new EPGSortDirection(EPGSortDirection::Descending)));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8)
		[
			SNew(SGridPanel)
			.FillColumn(1, 1.0f)

			// 파일 크기 필터
			+ SGridPanel::Slot(0, 0)
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FileSizeFilter", "파일 크기:"))
			]
			+ SGridPanel::Slot(1, 0)
			.Padding(4)
			[
				SAssignNew(FileSizeComboBox, SComboBox<TSharedPtr<EPGFileSizeFilter>>)
				.OptionsSource(&FileSizeOptions)
				.OnGenerateWidget(this, &SPGAdvancedFilterWidget::OnGenerateFileSizeWidget)
				.OnSelectionChanged(this, &SPGAdvancedFilterWidget::OnFileSizeFilterChanged)
				.InitiallySelectedItem(FileSizeOptions[0])
				[
					SNew(STextBlock)
					.Text(this, &SPGAdvancedFilterWidget::GetFileSizeText)
				]
			]

			// 정렬 기준
			+ SGridPanel::Slot(0, 1)
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SortBy", "정렬 기준:"))
			]
			+ SGridPanel::Slot(1, 1)
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SortCriteriaComboBox, SComboBox<TSharedPtr<EPGSortCriteria>>)
					.OptionsSource(&SortCriteriaOptions)
					.OnGenerateWidget(this, &SPGAdvancedFilterWidget::OnGenerateSortCriteriaWidget)
					.OnSelectionChanged(this, &SPGAdvancedFilterWidget::OnSortCriteriaChanged)
					.InitiallySelectedItem(SortCriteriaOptions[0])
					[
						SNew(STextBlock)
						.Text(this, &SPGAdvancedFilterWidget::GetSortCriteriaText)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8, 0, 0, 0)
				[
					SAssignNew(SortDirectionComboBox, SComboBox<TSharedPtr<EPGSortDirection>>)
					.OptionsSource(&SortDirectionOptions)
					.OnGenerateWidget(this, &SPGAdvancedFilterWidget::OnGenerateSortDirectionWidget)
					.OnSelectionChanged(this, &SPGAdvancedFilterWidget::OnSortDirectionChanged)
					.InitiallySelectedItem(SortDirectionOptions[0])
					[
						SNew(STextBlock)
						.Text(this, &SPGAdvancedFilterWidget::GetSortDirectionText)
					]
				]
			]

			// 경로 필터
			+ SGridPanel::Slot(0, 2)
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PathFilter", "경로 필터:"))
			]
			+ SGridPanel::Slot(1, 2)
			.Padding(4)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("PathFilterHint", "경로에 포함될 텍스트 입력..."))
				.Text_Lambda([this]() 
				{ 
					return AdvancedFilter ? FText::FromString(AdvancedFilter->PathFilter) : FText::GetEmpty(); 
				})
				.OnTextChanged_Lambda([this](const FText& NewText) 
				{ 
					if (AdvancedFilter) AdvancedFilter->PathFilter = NewText.ToString(); 
				})
			]

			// 이름 필터
			+ SGridPanel::Slot(0, 3)
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NameFilter", "이름 필터:"))
			]
			+ SGridPanel::Slot(1, 3)
			.Padding(4)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("NameFilterHint", "이름에 포함될 텍스트 입력..."))
				.Text_Lambda([this]() 
				{ 
					return AdvancedFilter ? FText::FromString(AdvancedFilter->NameFilter) : FText::GetEmpty(); 
				})
				.OnTextChanged_Lambda([this](const FText& NewText) 
				{ 
					if (AdvancedFilter) AdvancedFilter->NameFilter = NewText.ToString(); 
				})
			]
		]
	];
}

void SPGAdvancedFilterWidget::OnFileSizeFilterChanged(TSharedPtr<EPGFileSizeFilter> NewValue, ESelectInfo::Type SelectInfo)
{
	if (AdvancedFilter && NewValue.IsValid())
	{
		AdvancedFilter->FileSizeFilter = *NewValue;
	}
}

void SPGAdvancedFilterWidget::OnDateFilterChanged(TSharedPtr<EPGDateFilter> NewValue, ESelectInfo::Type SelectInfo)
{
	if (AdvancedFilter && NewValue.IsValid())
	{
		AdvancedFilter->DateFilter = *NewValue;
	}
}

void SPGAdvancedFilterWidget::OnSortCriteriaChanged(TSharedPtr<EPGSortCriteria> NewValue, ESelectInfo::Type SelectInfo)
{
	if (AdvancedFilter && NewValue.IsValid())
	{
		AdvancedFilter->SortCriteria = *NewValue;
	}
}

void SPGAdvancedFilterWidget::OnSortDirectionChanged(TSharedPtr<EPGSortDirection> NewValue, ESelectInfo::Type SelectInfo)
{
	if (AdvancedFilter && NewValue.IsValid())
	{
		AdvancedFilter->SortDirection = *NewValue;
	}
}

TSharedRef<SWidget> SPGAdvancedFilterWidget::OnGenerateFileSizeWidget(TSharedPtr<EPGFileSizeFilter> InOption)
{
	FText OptionText;
	if (InOption.IsValid())
	{
		switch (*InOption)
		{
		case EPGFileSizeFilter::All:
			OptionText = LOCTEXT("FileSizeAll", "모든 크기");
			break;
		case EPGFileSizeFilter::Small:
			OptionText = LOCTEXT("FileSizeSmall", "작은 파일 (< 1MB)");
			break;
		case EPGFileSizeFilter::Medium:
			OptionText = LOCTEXT("FileSizeMedium", "중간 파일 (1MB - 10MB)");
			break;
		case EPGFileSizeFilter::Large:
			OptionText = LOCTEXT("FileSizeLarge", "큰 파일 (> 10MB)");
			break;
		case EPGFileSizeFilter::Custom:
			OptionText = LOCTEXT("FileSizeCustom", "사용자 정의");
			break;
		}
	}
	return SNew(STextBlock).Text(OptionText);
}

TSharedRef<SWidget> SPGAdvancedFilterWidget::OnGenerateDateFilterWidget(TSharedPtr<EPGDateFilter> InOption)
{
	FText OptionText;
	if (InOption.IsValid())
	{
		switch (*InOption)
		{
		case EPGDateFilter::All:
			OptionText = LOCTEXT("DateFilterAll", "모든 날짜");
			break;
		case EPGDateFilter::Today:
			OptionText = LOCTEXT("DateFilterToday", "오늘");
			break;
		case EPGDateFilter::LastWeek:
			OptionText = LOCTEXT("DateFilterLastWeek", "지난 주");
			break;
		case EPGDateFilter::LastMonth:
			OptionText = LOCTEXT("DateFilterLastMonth", "지난 달");
			break;
		case EPGDateFilter::LastYear:
			OptionText = LOCTEXT("DateFilterLastYear", "지난 해");
			break;
		case EPGDateFilter::Custom:
			OptionText = LOCTEXT("DateFilterCustom", "사용자 정의");
			break;
		}
	}
	return SNew(STextBlock).Text(OptionText);
}

TSharedRef<SWidget> SPGAdvancedFilterWidget::OnGenerateSortCriteriaWidget(TSharedPtr<EPGSortCriteria> InOption)
{
	FText OptionText;
	if (InOption.IsValid())
	{
		switch (*InOption)
		{
		case EPGSortCriteria::Name:
			OptionText = LOCTEXT("SortByName", "이름");
			break;
		case EPGSortCriteria::ParentClass:
			OptionText = LOCTEXT("SortByParentClass", "부모 클래스");
			break;
		case EPGSortCriteria::Path:
			OptionText = LOCTEXT("SortByPath", "경로");
			break;
		case EPGSortCriteria::Size:
			OptionText = LOCTEXT("SortBySize", "크기");
			break;
		case EPGSortCriteria::LastModified:
			OptionText = LOCTEXT("SortByLastModified", "수정일");
			break;
		}
	}
	return SNew(STextBlock).Text(OptionText);
}

TSharedRef<SWidget> SPGAdvancedFilterWidget::OnGenerateSortDirectionWidget(TSharedPtr<EPGSortDirection> InOption)
{
	FText OptionText;
	if (InOption.IsValid())
	{
		switch (*InOption)
		{
		case EPGSortDirection::Ascending:
			OptionText = LOCTEXT("SortAscending", "오름차순");
			break;
		case EPGSortDirection::Descending:
			OptionText = LOCTEXT("SortDescending", "내림차순");
			break;
		}
	}
	return SNew(STextBlock).Text(OptionText);
}

FText SPGAdvancedFilterWidget::GetFileSizeText() const
{
	if (!AdvancedFilter)
	{
		return LOCTEXT("FileSizeAll", "모든 크기");
	}

	switch (AdvancedFilter->FileSizeFilter)
	{
	case EPGFileSizeFilter::All:
		return LOCTEXT("FileSizeAll", "모든 크기");
	case EPGFileSizeFilter::Small:
		return LOCTEXT("FileSizeSmall", "작은 파일 (< 1MB)");
	case EPGFileSizeFilter::Medium:
		return LOCTEXT("FileSizeMedium", "중간 파일 (1MB - 10MB)");
	case EPGFileSizeFilter::Large:
		return LOCTEXT("FileSizeLarge", "큰 파일 (> 10MB)");
	case EPGFileSizeFilter::Custom:
		return LOCTEXT("FileSizeCustom", "사용자 정의");
	default:
		return LOCTEXT("FileSizeAll", "모든 크기");
	}
}

FText SPGAdvancedFilterWidget::GetDateFilterText() const
{
	if (!AdvancedFilter)
	{
		return LOCTEXT("DateFilterAll", "모든 날짜");
	}

	switch (AdvancedFilter->DateFilter)
	{
	case EPGDateFilter::All:
		return LOCTEXT("DateFilterAll", "모든 날짜");
	case EPGDateFilter::Today:
		return LOCTEXT("DateFilterToday", "오늘");
	case EPGDateFilter::LastWeek:
		return LOCTEXT("DateFilterLastWeek", "지난 주");
	case EPGDateFilter::LastMonth:
		return LOCTEXT("DateFilterLastMonth", "지난 달");
	case EPGDateFilter::LastYear:
		return LOCTEXT("DateFilterLastYear", "지난 해");
	case EPGDateFilter::Custom:
		return LOCTEXT("DateFilterCustom", "사용자 정의");
	default:
		return LOCTEXT("DateFilterAll", "모든 날짜");
	}
}

FText SPGAdvancedFilterWidget::GetSortCriteriaText() const
{
	if (!AdvancedFilter)
	{
		return LOCTEXT("SortByName", "이름");
	}

	switch (AdvancedFilter->SortCriteria)
	{
	case EPGSortCriteria::Name:
		return LOCTEXT("SortByName", "이름");
	case EPGSortCriteria::ParentClass:
		return LOCTEXT("SortByParentClass", "부모 클래스");
	case EPGSortCriteria::Path:
		return LOCTEXT("SortByPath", "경로");
	case EPGSortCriteria::Size:
		return LOCTEXT("SortBySize", "크기");
	case EPGSortCriteria::LastModified:
		return LOCTEXT("SortByLastModified", "수정일");
	default:
		return LOCTEXT("SortByName", "이름");
	}
}

FText SPGAdvancedFilterWidget::GetSortDirectionText() const
{
	if (!AdvancedFilter)
	{
		return LOCTEXT("SortAscending", "오름차순");
	}

	switch (AdvancedFilter->SortDirection)
	{
	case EPGSortDirection::Ascending:
		return LOCTEXT("SortAscending", "오름차순");
	case EPGSortDirection::Descending:
		return LOCTEXT("SortDescending", "내림차순");
	default:
		return LOCTEXT("SortAscending", "오름차순");
	}
}

// =============================================
// 히스토리 위젯 구현
// =============================================

/**
 * 히스토리 아이템 행 위젯
 */
class SPGHistoryItemRow : public SMultiColumnTableRow<TSharedPtr<FPGSearchHistoryItem>>
{
public:
	SLATE_BEGIN_ARGS(SPGHistoryItemRow) {}
		SLATE_ARGUMENT(TSharedPtr<FPGSearchHistoryItem>, HistoryItem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		HistoryItem = InArgs._HistoryItem;
		SMultiColumnTableRow<TSharedPtr<FPGSearchHistoryItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (!HistoryItem.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		if (ColumnName == "Time")
		{
			return SNew(STextBlock)
				.Text(FText::FromString(HistoryItem->SearchTime.ToString(TEXT("%m/%d %H:%M"))))
				.Font(FAppStyle::GetFontStyle("SmallFont"));
		}
		else if (ColumnName == "Class")
		{
			FString ClassName = HistoryItem->SearchCriteria.ParentClass ? 
				HistoryItem->SearchCriteria.ParentClass->GetName() : TEXT("None");
			return SNew(STextBlock)
				.Text(FText::FromString(ClassName))
				.Font(FAppStyle::GetFontStyle("SmallFont"));
		}
		else if (ColumnName == "Results")
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(HistoryItem->ResultCount))
				.Font(FAppStyle::GetFontStyle("SmallFont"));
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FPGSearchHistoryItem> HistoryItem;
};

void SPGSearchHistoryWidget::Construct(const FArguments& InArgs)
{
	SearchTool = InArgs._SearchTool;
	OnHistoryItemSelected = InArgs._OnHistoryItemSelected;

	ChildSlot
	[
		SNew(SVerticalBox)

		// 헤더
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HistoryTitle", "최근 검색"))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.Text(LOCTEXT("ClearHistory", "지우기"))
				.ToolTipText(LOCTEXT("ClearHistoryTooltip", "검색 히스토리를 모두 지웁니다"))
				.OnClicked(this, &SPGSearchHistoryWidget::OnClearHistoryClicked)
			]
		]

		// 히스토리 리스트 (간단한 텍스트 리스트로 구현)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HistorySimple", "검색 히스토리 표시"))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
		]
	];

	RefreshHistory();
}

void SPGSearchHistoryWidget::RefreshHistory()
{
	// 간단한 구현
}

TSharedRef<ITableRow> SPGSearchHistoryWidget::OnGenerateHistoryRow(TSharedPtr<FPGSearchHistoryItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPGHistoryItemRow, OwnerTable)
		.HistoryItem(Item);
}

void SPGSearchHistoryWidget::OnHistoryDoubleClicked(TSharedPtr<FPGSearchHistoryItem> Item)
{
	if (Item.IsValid())
	{
		OnHistoryItemSelected.ExecuteIfBound();
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("History item selected: %s"), *Item->GetDisplayText());
	}
}

FReply SPGSearchHistoryWidget::OnClearHistoryClicked()
{
	if (SearchTool)
	{
		SearchTool->ClearSearchHistory();
		RefreshHistory();
	}
	return FReply::Handled();
}

// =============================================
// 북마크 위젯 구현
// =============================================

void SPGBookmarkWidget::Construct(const FArguments& InArgs)
{
	SearchTool = InArgs._SearchTool;
	OnBookmarkSelected = InArgs._OnBookmarkSelected;

	ChildSlot
	[
		SNew(SVerticalBox)

		// 헤더
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BookmarksTitle", "저장된 검색"))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]
		]

		// 북마크 리스트 (간단한 텍스트로 구현)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BookmarkSimple", "북마크 목록 표시"))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
		]
	];

	RefreshBookmarks();
}

void SPGBookmarkWidget::RefreshBookmarks()
{
	// 간단한 구현
}

TSharedRef<ITableRow> SPGBookmarkWidget::OnGenerateBookmarkRow(TSharedPtr<FPGBookmarkItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPGBookmarkItem>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(Item.IsValid() ? FText::FromString(Item->BookmarkName) : FText::GetEmpty())
	];
}

void SPGBookmarkWidget::OnBookmarkDoubleClicked(TSharedPtr<FPGBookmarkItem> Item)
{
	if (Item.IsValid())
	{
		OnBookmarkSelected.ExecuteIfBound();
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("Bookmark selected: %s"), *Item->BookmarkName);
	}
}

FReply SPGBookmarkWidget::OnAddBookmarkClicked()
{
	return FReply::Handled();
}

FReply SPGBookmarkWidget::OnRemoveBookmarkClicked()
{
	return FReply::Handled();
}

// =============================================
// 내보내기 위젯 구현
// =============================================

void SPGExportWidget::Construct(const FArguments& InArgs)
{
	SearchResults = InArgs._SearchResults;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8)
		[
			SNew(SVerticalBox)

			// 제목
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ExportTitle", "검색 결과 내보내기"))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]

			// CSV 내보내기
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExportCSV", "CSV 파일로 내보내기"))
				.ToolTipText(LOCTEXT("ExportCSVTooltip", "검색 결과를 CSV 형식으로 저장합니다"))
				.OnClicked(this, &SPGExportWidget::OnExportCSVClicked)
				.IsEnabled_Lambda([this]() 
				{ 
					return SearchResults && SearchResults->Num() > 0; 
				})
			]

			// 상태 텍스트
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() 
				{ 
					if (!SearchResults || SearchResults->Num() == 0)
					{
						return LOCTEXT("NoResults", "내보낼 검색 결과가 없습니다");
					}
					return FText::Format(LOCTEXT("ResultCount", "{0}개 결과 준비됨"), 
						FText::AsNumber(SearchResults->Num()));
				})
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ColorAndOpacity_Lambda([this]() 
				{ 
					return (SearchResults && SearchResults->Num() > 0) ? 
						FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground();
				})
			]
		]
	];
}

FReply SPGExportWidget::OnExportCSVClicked()
{
	// 간단한 구현 - 실제로는 파일 다이얼로그와 CSV 생성이 필요
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("Export CSV clicked"));
	return FReply::Handled();
}

FReply SPGExportWidget::OnExportJSONClicked()
{
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("Export JSON clicked"));
	return FReply::Handled();
}

FReply SPGExportWidget::OnExportTextClicked()
{
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("Export Text clicked"));
	return FReply::Handled();
}

void SPGExportWidget::ExportToCSV(const FString& FilePath)
{
	// 간단한 구현
}

void SPGExportWidget::ExportToJSON(const FString& FilePath)
{
	// 간단한 구현
}

void SPGExportWidget::ExportToText(const FString& FilePath)
{
	// 간단한 구현
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR

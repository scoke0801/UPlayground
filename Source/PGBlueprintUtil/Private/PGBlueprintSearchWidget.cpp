#include "PGBlueprintSearchWidget.h"

#if WITH_EDITOR
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "ClassViewerModule.h"
#include "Styling/AppStyle.h"
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

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR

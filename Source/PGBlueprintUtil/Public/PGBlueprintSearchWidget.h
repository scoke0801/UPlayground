#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Framework/Views/TableViewMetadata.h"
#include "PGBlueprintSearchTypes.h"
#include "PGBlueprintSearchTool.h"

class SClassViewer;
class SExpandableArea;
class SPGAdvancedFilterWidget;
class SPGSearchHistoryWidget;
class SPGBookmarkWidget;
class SPGExportWidget;

/**
 * 검색 결과 리스트 아이템 위젯
 */
class SPGBlueprintSearchResultItem : public SMultiColumnTableRow<TSharedPtr<FPGBlueprintSearchResult>>
{
public:
	SLATE_BEGIN_ARGS(SPGBlueprintSearchResultItem) {}
		SLATE_ARGUMENT(TSharedPtr<FPGBlueprintSearchResult>, SearchResult)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	// SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FPGBlueprintSearchResult> SearchResult;
};

/**
 * 클래스 선택을 위한 필터
 */
class FPGBlueprintParentClassFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override;
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override;
};

/**
 * 고급 필터 설정 위젯
 */
class SPGAdvancedFilterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPGAdvancedFilterWidget) {}
		SLATE_ARGUMENT(FPGAdvancedFilter*, AdvancedFilter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FPGAdvancedFilter* AdvancedFilter = nullptr;

	// UI 컴포넌트들
	TSharedPtr<SComboBox<TSharedPtr<EPGFileSizeFilter>>> FileSizeComboBox;
	TSharedPtr<SComboBox<TSharedPtr<EPGDateFilter>>> DateFilterComboBox;
	TSharedPtr<SComboBox<TSharedPtr<EPGSortCriteria>>> SortCriteriaComboBox;
	TSharedPtr<SComboBox<TSharedPtr<EPGSortDirection>>> SortDirectionComboBox;

	// 옵션 배열들
	TArray<TSharedPtr<EPGFileSizeFilter>> FileSizeOptions;
	TArray<TSharedPtr<EPGDateFilter>> DateFilterOptions;
	TArray<TSharedPtr<EPGSortCriteria>> SortCriteriaOptions;
	TArray<TSharedPtr<EPGSortDirection>> SortDirectionOptions;

	// 이벤트 핸들러들
	void OnFileSizeFilterChanged(TSharedPtr<EPGFileSizeFilter> NewValue, ESelectInfo::Type SelectInfo);
	void OnDateFilterChanged(TSharedPtr<EPGDateFilter> NewValue, ESelectInfo::Type SelectInfo);
	void OnSortCriteriaChanged(TSharedPtr<EPGSortCriteria> NewValue, ESelectInfo::Type SelectInfo);
	void OnSortDirectionChanged(TSharedPtr<EPGSortDirection> NewValue, ESelectInfo::Type SelectInfo);

	// 콤보박스 텍스트 생성
	TSharedRef<SWidget> OnGenerateFileSizeWidget(TSharedPtr<EPGFileSizeFilter> InOption);
	TSharedRef<SWidget> OnGenerateDateFilterWidget(TSharedPtr<EPGDateFilter> InOption);
	TSharedRef<SWidget> OnGenerateSortCriteriaWidget(TSharedPtr<EPGSortCriteria> InOption);
	TSharedRef<SWidget> OnGenerateSortDirectionWidget(TSharedPtr<EPGSortDirection> InOption);

	FText GetFileSizeText() const;
	FText GetDateFilterText() const;
	FText GetSortCriteriaText() const;
	FText GetSortDirectionText() const;
};

/**
 * 검색 히스토리 위젯
 */
class SPGSearchHistoryWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPGSearchHistoryWidget) {}
		SLATE_ARGUMENT(TObjectPtr<UPGBlueprintSearchTool>, SearchTool)
		SLATE_EVENT(FSimpleDelegate, OnHistoryItemSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void RefreshHistory();

private:
	TObjectPtr<UPGBlueprintSearchTool> SearchTool;
	TSharedPtr<SListView<TSharedPtr<FPGSearchHistoryItem>>> HistoryListView;
	TArray<TSharedPtr<FPGSearchHistoryItem>> HistoryItems;
	FSimpleDelegate OnHistoryItemSelected;

	// 이벤트 핸들러들
	TSharedRef<ITableRow> OnGenerateHistoryRow(TSharedPtr<FPGSearchHistoryItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnHistoryDoubleClicked(TSharedPtr<FPGSearchHistoryItem> Item);
	FReply OnClearHistoryClicked();
};

/**
 * 북마크 관리 위젯
 */
class SPGBookmarkWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPGBookmarkWidget) {}
		SLATE_ARGUMENT(TObjectPtr<UPGBlueprintSearchTool>, SearchTool)
		SLATE_EVENT(FSimpleDelegate, OnBookmarkSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void RefreshBookmarks();

private:
	TObjectPtr<UPGBlueprintSearchTool> SearchTool;
	TSharedPtr<SListView<TSharedPtr<FPGBookmarkItem>>> BookmarkListView;
	TArray<TSharedPtr<FPGBookmarkItem>> BookmarkItems;
	FSimpleDelegate OnBookmarkSelected;

	// 이벤트 핸들러들
	TSharedRef<ITableRow> OnGenerateBookmarkRow(TSharedPtr<FPGBookmarkItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnBookmarkDoubleClicked(TSharedPtr<FPGBookmarkItem> Item);
	FReply OnAddBookmarkClicked();
	FReply OnRemoveBookmarkClicked();
};

/**
 * 결과 내보내기 위젯
 */
class SPGExportWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPGExportWidget) {}
		SLATE_ARGUMENT(TArray<TSharedPtr<FPGBlueprintSearchResult>>*, SearchResults)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TArray<TSharedPtr<FPGBlueprintSearchResult>>* SearchResults = nullptr;

	// 이벤트 핸들러들
	FReply OnExportCSVClicked();
	FReply OnExportJSONClicked();
	FReply OnExportTextClicked();

	// 내보내기 헬퍼
	void ExportToCSV(const FString& FilePath);
	void ExportToJSON(const FString& FilePath);
	void ExportToText(const FString& FilePath);
};

/**
 * 블루프린트 검색 메인 위젯
 */
class PGBLUEPRINTUTIL_API SPGBlueprintSearchWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPGBlueprintSearchWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPGBlueprintSearchWidget();

	/** 위젯 크기 권장값 */
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1000, 700); }

	/** 컬럼 헤더 */
	static const FName ColumnName_Name;
	static const FName ColumnName_ParentClass;
	static const FName ColumnName_Path;
	static const FName ColumnName_Size;
	static const FName ColumnName_LastModified;

private:
	/** UI 컴포넌트들 */
	TSharedPtr<SListView<TSharedPtr<FPGBlueprintSearchResult>>> ResultListView;
	TSharedPtr<SSearchBox> FilterSearchBox;
	TSharedPtr<SComboBox<TSharedPtr<EPGSearchScope>>> ScopeComboBox;
	TSharedPtr<SPGAdvancedFilterWidget> AdvancedFilterWidget;
	TSharedPtr<SPGSearchHistoryWidget> HistoryWidget;
	TSharedPtr<SPGBookmarkWidget> BookmarkWidget;
	TSharedPtr<SPGExportWidget> ExportWidget;
	TSharedPtr<SExpandableArea> AdvancedFiltersArea;
	TSharedPtr<SExpandableArea> HistoryArea;
	TSharedPtr<SExpandableArea> BookmarksArea;
	
	/** 검색 관련 */
	TObjectPtr<UPGBlueprintSearchTool> SearchTool;
	FPGBlueprintSearchCriteria CurrentCriteria;
	TArray<TSharedPtr<FPGBlueprintSearchResult>> SearchResults;
	TArray<TSharedPtr<FPGBlueprintSearchResult>> FilteredResults;
	
	/** 검색 범위 옵션들 */
	TArray<TSharedPtr<EPGSearchScope>> SearchScopeOptions;
	
	/** 현재 선택된 클래스 */
	UClass* SelectedClass = nullptr;

	/** UI 생성 메서드들 */
	TSharedRef<SWidget> CreateClassSelectionArea();
	TSharedRef<SWidget> CreateSearchOptionsArea();
	TSharedRef<SWidget> CreateAdvancedOptionsArea();
	TSharedRef<SWidget> CreateResultsArea();
	TSharedRef<SWidget> CreateStatusArea();
	TSharedRef<SWidget> CreateSidePanel();

	/** 이벤트 핸들러들 */
	void OnClassSelected(UClass* InClass);
	FReply OnSearchClicked();
	FReply OnClearClicked();
	FReply OnAddBookmarkClicked();
	void OnSearchCompleted(const TArray<FPGBlueprintSearchResult>& Results);
	void OnSearchProgressUpdated(float Progress);
	void OnFilterTextChanged(const FText& InFilterText);
	void OnResultDoubleClicked(TSharedPtr<FPGBlueprintSearchResult> Item);
	TSharedPtr<SWidget> OnResultRightClicked(TSharedPtr<FPGBlueprintSearchResult> Item);

	/** 헬퍼 메서드들 */
	void UpdateFilteredResults();
	bool DoesResultMatchFilter(const TSharedPtr<FPGBlueprintSearchResult>& Result, const FString& FilterText) const;
	FText GetSearchButtonText() const;
	bool IsSearchButtonEnabled() const;
	FText GetStatusText() const;
	EVisibility GetProgressBarVisibility() const;
	TOptional<float> GetProgressPercent() const;

	/** 검색 범위 콤보박스 */
	TSharedRef<SWidget> OnGenerateScopeWidget(TSharedPtr<EPGSearchScope> InOption);
	FText GetCurrentScopeText() const;
	void OnScopeChanged(TSharedPtr<EPGSearchScope> NewScope, ESelectInfo::Type SelectInfo);

	/** 리스트 뷰 관련 */
	TSharedRef<ITableRow> OnGenerateResultRow(TSharedPtr<FPGBlueprintSearchResult> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** 컬럼 헤더 */
	static const FName ColumnName_Name;
	static const FName ColumnName_ParentClass;
	static const FName ColumnName_Path;
	static const FName ColumnName_Size;
	static const FName ColumnName_LastModified;

private:

	/** 상태 추적 */
	float CurrentProgress = 0.0f;
	bool bIsSearching = false;
	FString CurrentFilterText;
};

#endif // WITH_EDITOR

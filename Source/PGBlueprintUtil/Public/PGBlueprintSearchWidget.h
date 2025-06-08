#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboBox.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Framework/Views/TableViewMetadata.h"
#include "PGBlueprintSearchTypes.h"
#include "PGBlueprintSearchTool.h"

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
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800, 600); }

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
	TSharedRef<SWidget> CreateResultsArea();
	TSharedRef<SWidget> CreateStatusArea();

	/** 이벤트 핸들러들 */
	void OnClassSelected(UClass* InClass);
	FReply OnSearchClicked();
	FReply OnClearClicked();
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

	/** 상태 추적 */
	float CurrentProgress = 0.0f;
	bool bIsSearching = false;
	FString CurrentFilterText;
};

#endif // WITH_EDITOR

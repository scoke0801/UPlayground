#include "PGBlueprintSearchTool.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Async/Async.h"
#include "PGBlueprintUtil.h"
#include "HAL/PlatformFilemanager.h"

UPGBlueprintSearchTool::UPGBlueprintSearchTool()
{
	bIsSearching = false;
	bCancelRequested = false;
}

TArray<FPGBlueprintSearchResult> UPGBlueprintSearchTool::SearchBlueprints(const FPGBlueprintSearchCriteria& SearchCriteria)
{
	if (!SearchCriteria.IsValid())
	{
		UE_LOG(LogPGBlueprintUtil, Warning, TEXT("SearchBlueprints: Invalid search criteria"));
		return TArray<FPGBlueprintSearchResult>();
	}

	// 시작 시간 기록
	FDateTime StartTime = FDateTime::Now();

	// 캐시 확인
	FString CacheKey = GenerateCacheKey(SearchCriteria);
	if (CachedResults.Contains(CacheKey))
	{
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Using cached results for key: %s"), *CacheKey);
		return CachedResults[CacheKey];
	}

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Starting search for parent class: %s"), 
		   SearchCriteria.ParentClass ? *SearchCriteria.ParentClass->GetName() : TEXT("None"));

	TArray<FPGBlueprintSearchResult> Results;

	// AssetRegistry 가져오기
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 블루프린트 에셋 필터 설정
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	// 검색 범위에 따른 패키지 경로 설정
	switch (SearchCriteria.SearchScope)
	{
	case EPGSearchScope::ProjectOnly:
		Filter.PackagePaths.Add("/Game");
		break;
	case EPGSearchScope::IncludePlugins:
		Filter.PackagePaths.Add("/Game");
		// 플러그인 경로들도 추가
		Filter.PackagePaths.Add("/Plugins");
		break;
	case EPGSearchScope::IncludeEngine:
		// 모든 경로 포함 (필터 없음)
		break;
	}

	// 에셋 검색
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Found %d total blueprint assets"), AssetDataList.Num());

	// 진행률 추적
	int32 ProcessedCount = 0;
	int32 TotalCount = AssetDataList.Num();

	// 각 에셋 검토
	for (const FAssetData& AssetData : AssetDataList)
	{
		// 취소 요청 확인
		if (bCancelRequested)
		{
			UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Search cancelled by user"));
			break;
		}

		// 검색 조건과 일치하는지 확인
		if (DoesAssetMatchCriteria(AssetData, SearchCriteria))
		{
			FPGBlueprintSearchResult Result = FPGBlueprintSearchResult::FromAssetData(AssetData, SearchCriteria.ParentClass);
			
			// 고급 필터 적용
			if (Result.MatchesAdvancedFilter(SearchCriteria.AdvancedFilter))
			{
				Results.Add(Result);
			}
		}

		ProcessedCount++;
		if (ProcessedCount % 50 == 0) // 50개마다 진행률 업데이트
		{
			float Progress = static_cast<float>(ProcessedCount) / TotalCount;
			OnSearchProgressUpdated.Broadcast(Progress);
		}
	}

	// 정렬 적용
	SortResults(Results, SearchCriteria.AdvancedFilter.SortCriteria, SearchCriteria.AdvancedFilter.SortDirection);

	// 캐시에 저장
	CachedResults.Add(CacheKey, Results);

	// 검색 완료 시간 계산
	FDateTime EndTime = FDateTime::Now();
	float SearchDuration = (EndTime - StartTime).GetTotalSeconds();

	// 검색 히스토리에 추가
	AddToSearchHistory(SearchCriteria, Results.Num(), SearchDuration);

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Search completed. Found %d matching blueprints in %.2f seconds"), 
		   Results.Num(), SearchDuration);

	return Results;
}

void UPGBlueprintSearchTool::SearchBlueprintsAsync(const FPGBlueprintSearchCriteria& SearchCriteria)
{
	if (bIsSearching)
	{
		UE_LOG(LogPGBlueprintUtil, Warning, TEXT("SearchBlueprintsAsync: Search already in progress"));
		return;
	}

	bIsSearching = true;
	bCancelRequested = false;

	// 비동기 작업 시작
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, SearchCriteria]()
	{
		PerformAsyncSearch(SearchCriteria);
	});
}

void UPGBlueprintSearchTool::CancelSearch()
{
	if (bIsSearching)
	{
		bCancelRequested = true;
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("CancelSearch: Search cancellation requested"));
	}
}

bool UPGBlueprintSearchTool::AddBookmark(const FPGBookmarkItem& Bookmark)
{
	if (!Bookmark.IsValid())
	{
		UE_LOG(LogPGBlueprintUtil, Warning, TEXT("AddBookmark: Invalid bookmark"));
		return false;
	}

	// 같은 이름의 북마크가 있는지 확인
	for (const FPGBookmarkItem& ExistingBookmark : Bookmarks)
	{
		if (ExistingBookmark.BookmarkName == Bookmark.BookmarkName)
		{
			UE_LOG(LogPGBlueprintUtil, Warning, TEXT("AddBookmark: Bookmark with name '%s' already exists"), *Bookmark.BookmarkName);
			return false;
		}
	}

	Bookmarks.Add(Bookmark);
	UE_LOG(LogPGBlueprintUtil, Log, TEXT("AddBookmark: Added bookmark '%s'"), *Bookmark.BookmarkName);
	return true;
}

bool UPGBlueprintSearchTool::RemoveBookmark(const FString& BookmarkName)
{
	int32 RemovedCount = Bookmarks.RemoveAll([&BookmarkName](const FPGBookmarkItem& Item)
	{
		return Item.BookmarkName == BookmarkName;
	});

	if (RemovedCount > 0)
	{
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("RemoveBookmark: Removed bookmark '%s'"), *BookmarkName);
		return true;
	}

	UE_LOG(LogPGBlueprintUtil, Warning, TEXT("RemoveBookmark: Bookmark '%s' not found"), *BookmarkName);
	return false;
}

bool UPGBlueprintSearchTool::DoesAssetMatchCriteria(const FAssetData& AssetData, const FPGBlueprintSearchCriteria& SearchCriteria) const
{
	// 에셋 경로가 검색 범위에 포함되는지 확인
	if (!IsAssetInSearchScope(AssetData.PackageName, SearchCriteria.SearchScope))
	{
		return false;
	}

	// 블루프린트 부모 클래스 가져오기
	UClass* BlueprintParentClass = GetBlueprintParentClass(AssetData);
	if (!BlueprintParentClass)
	{
		return false;
	}

	// 부모 클래스 일치 확인
	if (SearchCriteria.bExactMatchOnly)
	{
		// 정확한 일치만
		return BlueprintParentClass == SearchCriteria.ParentClass;
	}
	else
	{
		// 상속 계층 포함
		return BlueprintParentClass->IsChildOf(SearchCriteria.ParentClass);
	}
}

UClass* UPGBlueprintSearchTool::GetBlueprintParentClass(const FAssetData& AssetData) const
{
	// Blueprint의 ParentClass 태그에서 부모 클래스 정보 가져오기
	FAssetTagValueRef ParentClassRef = AssetData.TagsAndValues.FindTag("ParentClass");
	if (!ParentClassRef.IsSet() || ParentClassRef.AsString().IsEmpty())
	{
		return nullptr;
	}

	// 클래스 경로에서 실제 클래스 찾기
	UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassRef.AsString());
	return ParentClass;
}

bool UPGBlueprintSearchTool::IsAssetInSearchScope(const FName& PackageName, EPGSearchScope SearchScope) const
{
	FString PackageNameStr = PackageName.ToString();

	switch (SearchScope)
	{
	case EPGSearchScope::ProjectOnly:
		return PackageNameStr.StartsWith(TEXT("/Game/"));
		
	case EPGSearchScope::IncludePlugins:
		return PackageNameStr.StartsWith(TEXT("/Game/")) || 
			   PackageNameStr.StartsWith(TEXT("/Plugins/")) ||
			   PackageNameStr.Contains(TEXT("Plugins"));
		
	case EPGSearchScope::IncludeEngine:
		return true; // 모든 경로 포함
		
	default:
		return false;
	}
}

void UPGBlueprintSearchTool::PerformAsyncSearch(FPGBlueprintSearchCriteria SearchCriteria)
{
	// 백그라운드에서 검색 수행
	TArray<FPGBlueprintSearchResult> Results = SearchBlueprints(SearchCriteria);

	// 메인 스레드에서 완료 이벤트 호출
	AsyncTask(ENamedThreads::GameThread, [this, Results]()
	{
		bIsSearching = false;
		bCancelRequested = false;
		OnSearchCompleted.Broadcast(Results);
	});
}

FString UPGBlueprintSearchTool::GenerateCacheKey(const FPGBlueprintSearchCriteria& SearchCriteria) const
{
	FString Key = FString::Printf(TEXT("%s_%d_%d_%d_%d_%d_%d_%s_%s"),
		SearchCriteria.ParentClass ? *SearchCriteria.ParentClass->GetName() : TEXT("None"),
		static_cast<int32>(SearchCriteria.SearchScope),
		SearchCriteria.bIncludeAbstractClasses ? 1 : 0,
		SearchCriteria.bIncludeDeprecated ? 1 : 0,
		SearchCriteria.bExactMatchOnly ? 1 : 0,
		static_cast<int32>(SearchCriteria.AdvancedFilter.FileSizeFilter),
		static_cast<int32>(SearchCriteria.AdvancedFilter.DateFilter),
		*SearchCriteria.AdvancedFilter.PathFilter,
		*SearchCriteria.AdvancedFilter.NameFilter
	);
	
	return FMD5::HashAnsiString(*Key);
}

void UPGBlueprintSearchTool::AddToSearchHistory(const FPGBlueprintSearchCriteria& SearchCriteria, int32 ResultCount, float Duration)
{
	FPGSearchHistoryItem HistoryItem;
	HistoryItem.SearchCriteria = SearchCriteria;
	HistoryItem.ResultCount = ResultCount;
	HistoryItem.SearchDuration = Duration;
	HistoryItem.SearchTime = FDateTime::Now();

	// 히스토리 앞에 추가 (최신순)
	SearchHistory.Insert(HistoryItem, 0);

	// 히스토리 최대 개수 제한 (예: 50개)
	const int32 MaxHistoryItems = 50;
	if (SearchHistory.Num() > MaxHistoryItems)
	{
		SearchHistory.RemoveAt(MaxHistoryItems, SearchHistory.Num() - MaxHistoryItems);
	}
}

void UPGBlueprintSearchTool::SortResults(TArray<FPGBlueprintSearchResult>& Results, EPGSortCriteria SortCriteria, EPGSortDirection SortDirection) const
{
	auto GetSortValue = [SortCriteria](const FPGBlueprintSearchResult& Item) -> FString
	{
		switch (SortCriteria)
		{
		case EPGSortCriteria::Name:
			return Item.BlueprintName;
		case EPGSortCriteria::ParentClass:
			return Item.ParentClassName;
		case EPGSortCriteria::Path:
			return Item.AssetPath;
		case EPGSortCriteria::Size:
			return FString::Printf(TEXT("%016lld"), Item.FileSize); // 숫자 정렬을 위해 패딩
		case EPGSortCriteria::LastModified:
			return Item.LastModified.ToString();
		default:
			return Item.BlueprintName;
		}
	};

	if (SortDirection == EPGSortDirection::Ascending)
	{
		Results.Sort([&GetSortValue](const FPGBlueprintSearchResult& A, const FPGBlueprintSearchResult& B)
		{
			return GetSortValue(A) < GetSortValue(B);
		});
	}
	else
	{
		Results.Sort([&GetSortValue](const FPGBlueprintSearchResult& A, const FPGBlueprintSearchResult& B)
		{
			return GetSortValue(A) > GetSortValue(B);
		});
	}
}

#endif // WITH_EDITOR
